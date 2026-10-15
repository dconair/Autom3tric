#include "autodj/engine/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "autodj/core/Denormals.h"
#include "autodj/core/RtGuard.h"

namespace autodj {

Engine::Engine(const EngineConfig& cfg) : cfg_(cfg) {
  cfg_.maxBlockFrames = std::clamp(cfg_.maxBlockFrames, 16, kMaxBlockFrames);
  cfg_.granularVoices = std::clamp(cfg_.granularVoices, 1, granular::kMaxVoices);

  ParamSpec bpm;
  bpm.key = "transport.bpm";
  bpm.label = "Tempo";
  bpm.group = "Transport";
  bpm.unit = "BPM";
  bpm.min = 40.f;
  bpm.max = 220.f;
  bpm.def = 120.f;
  ids_.bpm = reg_.add(std::move(bpm));

  ParamSpec mg;
  mg.key = "master.gain";
  mg.label = "Master";
  mg.group = "Master";
  mg.min = 0.f;
  mg.max = 1.5f;
  mg.def = 0.8f;
  mg.smoothMs = 20.f;
  ids_.masterGain = reg_.add(std::move(mg));

  ids_.tone = registerToneParams(reg_);
  ids_.granular = granular::registerGranularParams(reg_, cfg_.granularVoices);
  ids_.dj = dj::registerDjParams(reg_);
  ids_.perf = perf::registerPerfParams(reg_);
  reg_.freeze();
  params_ = std::make_unique<ParamStore>(reg_);

  const ProcessSpec spec{static_cast<double>(cfg_.sampleRate), cfg_.maxBlockFrames};
  tone_ = std::make_unique<ToneNode>(ids_.tone);
  tone_->prepare(spec);
  granular_ = std::make_unique<granular::GranularNode>(ids_.granular, cfg_.seed);
  granular_->prepare(spec);
  granular_->attachViz(&vizBuf_);
  dj_ = std::make_unique<dj::DjNode>(ids_.dj);
  dj_->prepare(spec);
  allNodes_[0] = tone_.get();
  allNodes_[1] = granular_.get();
  pads_ = std::make_unique<perf::PadNode>(ids_.perf);
  pads_->prepare(spec);
  fx_ = std::make_unique<perf::MasterFx>(ids_.perf);
  fx_->prepare(spec.sampleRate);
  allNodes_[2] = dj_.get();
  allNodes_[3] = pads_.get();

  capFrames_ = static_cast<std::size_t>((kCaptureSeconds + 2.0) * cfg_.sampleRate);
  capRing_ = std::make_unique<std::atomic<float>[]>(capFrames_ * 2);

  transport_.prepare(cfg_.sampleRate);
  std::memset(nodeBuf_, 0, sizeof(nodeBuf_));

  auto g = std::make_unique<GraphDesc>();
  g->add(tone_.get(), 1.f);
  g->add(granular_.get(), 1.f);
  g->add(dj_.get(), 1.f);
  g->add(pads_.get(), 1.f);
  g->version = ++graphVersion_;
  graph_ = g.get();
  heldGraphs_.emplace(graph_, std::move(g));
}

Engine::~Engine() = default;

// ------------------------------------------------------------------------------------------
// Control side
// ------------------------------------------------------------------------------------------

bool Engine::post(const Command& cmd) {
  std::lock_guard<std::mutex> lk(postMutex_);
  return commands_.tryPush(cmd);
}

bool Engine::play() { return post(Command::make(CmdType::Play)); }
bool Engine::stop() { return post(Command::make(CmdType::Stop)); }

bool Engine::seekBeats(double beats) {
  Command c = Command::make(CmdType::Seek);
  c.payload.d = beats;
  return post(c);
}

bool Engine::noteOn(int voice, float pitchSemitones, float velocity) {
  Command c = Command::make(CmdType::NoteOn);
  c.target = static_cast<std::uint8_t>(voice);
  c.f0 = pitchSemitones;
  c.f1 = velocity;
  return post(c);
}

bool Engine::noteOff(int voice) {
  Command c = Command::make(CmdType::NoteOff);
  c.target = static_cast<std::uint8_t>(voice);
  return post(c);
}

bool Engine::resetSteps(int voice) {
  Command c = Command::make(CmdType::StepReset);
  c.target = static_cast<std::uint8_t>(voice);
  return post(c);
}

bool Engine::resetEngine() { return post(Command::make(CmdType::ResetEngine)); }

bool Engine::setSource(int slot, std::shared_ptr<const SampleBuffer> buffer) {
  if (!buffer) return false;
  const SampleBuffer* raw = buffer.get();
  {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    if (slot >= 0 && slot < 16) slotBuffers_[slot] = buffer;
    heldBuffers_[raw] = std::move(buffer);
  }
  Command c = Command::make(CmdType::PublishSource);
  c.slot = static_cast<std::uint16_t>(slot);
  c.payload.ptr = raw;
  if (post(c)) return true;
  std::lock_guard<std::mutex> lk(ownerMutex_);
  heldBuffers_.erase(raw);
  return false;
}

bool Engine::sourcePeaks(int slot, int columns, float* out) const {
  if (slot < 0 || slot >= 16) return false;
  std::shared_ptr<const SampleBuffer> b;
  {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    b = slotBuffers_[slot];
  }
  if (!b) return false;
  computePeaks(*b, columns, out);
  return true;
}

bool Engine::sourceInfo(int slot, double* seconds, int* sampleRate, std::string* name) const {
  if (slot < 0 || slot >= 16) return false;
  std::shared_ptr<const SampleBuffer> b;
  {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    b = slotBuffers_[slot];
  }
  if (!b) return false;
  if (seconds) *seconds = static_cast<double>(b->frames()) / static_cast<double>(b->sampleRate());
  if (sampleRate) *sampleRate = b->sampleRate();
  if (name) *name = b->name();
  return true;
}

int Engine::addTrack(std::shared_ptr<const SampleBuffer> audio, const std::string& name, const analysis::TrackAnalysis* cached,
                     const analysis::ProgressFn& progress) {
  if (!audio || audio->frames() <= 0) return 0;
  auto t = std::make_shared<Track>();
  t->name = name;
  t->audio = audio;
  if (cached != nullptr) {
    t->analysis = *cached;
    if (progress) progress(1.f);
  } else {
    t->analysis = analysis::analyzeTrack(audio->data(), audio->frames(), audio->sampleRate(), {}, progress);
  }
  t->peaks = analysis::PeakPyramid(*audio);
  std::lock_guard<std::mutex> lk(trackMutex_);
  t->id = nextTrackId_++;
  const int id = t->id;
  tracks_[id] = std::move(t);
  return id;
}

int Engine::addTrackStub(const std::string& name, const analysis::TrackAnalysis& a) {
  auto t = std::make_shared<Track>();
  t->name = name;
  t->analysis = a;
  std::lock_guard<std::mutex> lk(trackMutex_);
  t->id = nextTrackId_++;
  const int id = t->id;
  tracks_[id] = std::move(t);
  return id;
}

std::shared_ptr<const Track> Engine::track(int id) const {
  std::lock_guard<std::mutex> lk(trackMutex_);
  auto it = tracks_.find(id);
  return it == tracks_.end() ? nullptr : it->second;
}

void Engine::removeTrack(int id) {
  std::lock_guard<std::mutex> lk(trackMutex_);
  tracks_.erase(id);
}

std::size_t Engine::trackCount() const {
  std::lock_guard<std::mutex> lk(trackMutex_);
  return tracks_.size();
}

// ---- DJ ------------------------------------------------------------------------------------------------

bool Engine::deckLoad(int deck, std::shared_ptr<const Track> track, double startSec) {
  if (deck < 0 || deck > 1) return false;
  const Track* raw = track.get();
  if (raw != nullptr) {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    HeldTrack& h = heldTracks_[raw];
    h.track = std::move(track);
    ++h.refs;
  }
  Command c = Command::make(CmdType::DeckLoad);
  c.target = static_cast<std::uint8_t>(deck);
  c.f0 = static_cast<float>(startSec);
  c.payload.ptr = raw;
  if (post(c)) return true;
  if (raw != nullptr) {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    auto it = heldTracks_.find(raw);
    if (it != heldTracks_.end() && --it->second.refs <= 0) heldTracks_.erase(it);
  }
  return false;
}

bool Engine::deckPlay(int deck, bool play) {
  if (deck < 0 || deck > 1) return false;
  Command c = Command::make(CmdType::DeckPlay);
  c.target = static_cast<std::uint8_t>(deck);
  c.i0 = play ? 1 : 0;
  return post(c);
}

bool Engine::deckSeek(int deck, double seconds) {
  if (deck < 0 || deck > 1) return false;
  Command c = Command::make(CmdType::DeckSeek);
  c.target = static_cast<std::uint8_t>(deck);
  c.payload.d = seconds;
  return post(c);
}

std::shared_ptr<const dj::MixPlan> Engine::planMix(const std::vector<int>& trackIds, const dj::PlanOptions& options) const {
  std::vector<std::shared_ptr<const Track>> list;
  for (int id : trackIds) {
    if (auto t = track(id)) list.push_back(std::move(t));
  }
  return dj::planMix(list, options);
}

std::vector<int> Engine::orderTracks(const std::vector<int>& trackIds, dj::OrderMode mode) const {
  std::vector<std::shared_ptr<const Track>> list;
  std::vector<int> origin;
  for (std::size_t i = 0; i < trackIds.size(); ++i) {
    if (auto t = track(trackIds[i])) {
      list.push_back(std::move(t));
      origin.push_back(static_cast<int>(i));
    }
  }
  std::vector<int> out;
  for (int k : dj::orderTracks(list, mode)) out.push_back(origin[static_cast<std::size_t>(k)]);
  return out;
}

bool Engine::planStart(std::shared_ptr<const dj::MixPlan> plan) {
  if (!plan || plan->items.empty()) return false;
  const dj::MixPlan* raw = plan.get();
  {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    heldPlans_[raw] = std::move(plan);
  }
  Command c = Command::make(CmdType::PlanStart);
  c.payload.ptr = raw;
  if (post(c)) return true;
  std::lock_guard<std::mutex> lk(ownerMutex_);
  heldPlans_.erase(raw);
  return false;
}

bool Engine::padTrigger(int pad, float velocity) {
  if (pad < 0 || pad >= perf::kPads) return false;
  Command c = Command::make(CmdType::PadTrigger);
  c.target = static_cast<std::uint8_t>(pad);
  c.f0 = velocity;
  return post(c);
}

bool Engine::padSetRow(int pad, unsigned mask) {
  if (pad < 0 || pad >= perf::kPads) return false;
  Command c = Command::make(CmdType::PadRow);
  c.target = static_cast<std::uint8_t>(pad);
  c.i0 = static_cast<std::int32_t>(mask & 0xFFFFu);
  return post(c);
}

bool Engine::granularSetSteps(int voice, unsigned mask, std::uint64_t cells) {
  if (voice < 0 || voice >= cfg_.granularVoices) return false;
  Command c = Command::make(CmdType::GranularSteps);
  c.target = static_cast<std::uint8_t>(voice);
  c.i0 = static_cast<std::int32_t>(mask & 0xFFFFu);
  c.payload.u = cells;
  return post(c);
}

std::shared_ptr<const SampleBuffer> Engine::captureOutput(double seconds, const std::string& name) const {
  const std::uint64_t written = capWritten_.load(std::memory_order_acquire);
  const std::size_t want = static_cast<std::size_t>(std::clamp(seconds, 0.1, kCaptureSeconds) * cfg_.sampleRate);
  const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(want, written));
  if (n < static_cast<std::size_t>(0.1 * cfg_.sampleRate)) return nullptr;
  std::vector<float> tmp(n * 2);
  std::size_t pos = static_cast<std::size_t>((written - n) % capFrames_);
  for (std::size_t i = 0; i < n; ++i) {
    tmp[2 * i] = capRing_[2 * pos].load(std::memory_order_relaxed);
    tmp[2 * i + 1] = capRing_[2 * pos + 1].load(std::memory_order_relaxed);
    pos = pos + 1 == capFrames_ ? 0 : pos + 1;
  }
  return SampleBuffer::fromInterleavedStereo(tmp.data(), n, cfg_.sampleRate, name);
}

bool Engine::planStop() { return post(Command::make(CmdType::PlanStop)); }

bool Engine::readDjStatus(dj::DjStatus& out) {
  std::lock_guard<std::mutex> lk(readMutex_);
  return djBuf_.read(out);
}

std::size_t Engine::heldTrackCount() const {
  std::lock_guard<std::mutex> lk(ownerMutex_);
  return heldTracks_.size();
}

std::size_t Engine::heldPlanCount() const {
  std::lock_guard<std::mutex> lk(ownerMutex_);
  return heldPlans_.size();
}

bool Engine::publishGraph(std::unique_ptr<GraphDesc> graph) {
  if (!graph) return false;
  graph->version = ++graphVersion_;
  const GraphDesc* raw = graph.get();
  {
    std::lock_guard<std::mutex> lk(ownerMutex_);
    heldGraphs_[raw] = std::move(graph);
  }
  Command c = Command::make(CmdType::SwapGraph);
  c.payload.ptr = raw;
  if (post(c)) return true;
  std::lock_guard<std::mutex> lk(ownerMutex_);
  heldGraphs_.erase(raw);
  return false;
}

std::unique_ptr<GraphDesc> Engine::makeGraph(float toneGain, float granularGain) {
  auto g = std::make_unique<GraphDesc>();
  g->add(tone_.get(), toneGain);
  g->add(granular_.get(), granularGain);
  g->add(dj_.get(), 1.f);
  g->add(pads_.get(), 1.f);
  return g;
}

std::size_t Engine::collectGarbage() {
  std::lock_guard<std::mutex> lk(ownerMutex_);
  std::size_t n = 0;
  GarbageMsg m;
  while (garbage_.tryPop(m)) {
    switch (m.kind) {
      case GarbageMsg::Kind::SampleBuffer: heldBuffers_.erase(static_cast<const SampleBuffer*>(m.ptr)); break;
      case GarbageMsg::Kind::Graph: heldGraphs_.erase(static_cast<const GraphDesc*>(m.ptr)); break;
      case GarbageMsg::Kind::Track: {
        auto it = heldTracks_.find(static_cast<const Track*>(m.ptr));
        if (it != heldTracks_.end() && --it->second.refs <= 0) heldTracks_.erase(it);
        break;
      }
      case GarbageMsg::Kind::Plan: heldPlans_.erase(static_cast<const dj::MixPlan*>(m.ptr)); break;
    }
    ++n;
  }
  return n;
}

std::size_t Engine::heldBufferCount() const {
  std::lock_guard<std::mutex> lk(ownerMutex_);
  return heldBuffers_.size();
}

std::size_t Engine::heldGraphCount() const {
  std::lock_guard<std::mutex> lk(ownerMutex_);
  return heldGraphs_.size();
}

bool Engine::readStatus(EngineStatus& out) {
  std::lock_guard<std::mutex> lk(readMutex_);
  return statusBuf_.read(out);
}

bool Engine::readGrainViz(GrainViz& out) {
  std::lock_guard<std::mutex> lk(readMutex_);
  return vizBuf_.read(out);
}

std::string Engine::exportPreset(const std::string& name) const { return presetToJson(reg_, *params_, name); }

LoadReport Engine::importPreset(const std::string& json) { return presetFromJson(reg_, *params_, json); }

// ------------------------------------------------------------------------------------------
// Audio side
// ------------------------------------------------------------------------------------------

void Engine::drainCommands() noexcept {
  Command c;
  for (int budget = 256; budget > 0 && commands_.tryPop(c); --budget) applyCommand(c);
}

void Engine::applyCommand(const Command& cmd) noexcept {
  switch (cmd.type) {
    case CmdType::Play: transport_.play(); return;
    case CmdType::Stop: transport_.stop(); return;
    case CmdType::Seek: transport_.seek(cmd.payload.d); return;
    case CmdType::SwapGraph: {
      const GraphDesc* old = graph_;
      graph_ = static_cast<const GraphDesc*>(cmd.payload.ptr);
      if (old) {
        GarbageMsg m;
        m.kind = GarbageMsg::Kind::Graph;
        m.ptr = old;
        (void)garbage_.tryPush(m);  // queue full: the control side still owns it and frees it on shutdown
      }
      return;
    }
    case CmdType::ResetEngine:
      transport_.reset();
      fx_->reset();
      for (Node* n : allNodes_) n->handleCommand(cmd, garbage_);
      return;
    default:
      for (Node* n : allNodes_) {
        if (n->handleCommand(cmd, garbage_)) return;
      }
      return;
  }
}

void Engine::process(float* out, int numFrames) noexcept {
  if (numFrames <= 0) return;
  rt::AudioThreadScope audioScope;
  ScopedFlushDenormals flushDenormals;
  const ClockFn clock = cfg_.clock ? cfg_.clock : &monotonicNowNs;
  const std::uint64_t t0 = cfg_.realtime ? clock() : 0;

  drainCommands();

  // Equal-sized chunks of at most maxBlockFrames: 480 -> 3 x 160, 192 -> 1 x 192, 96 -> 1 x 96.
  const int maxB = cfg_.maxBlockFrames;
  const int chunks = (numFrames + maxB - 1) / maxB;
  const int base = numFrames / chunks;
  const int extra = numFrames % chunks;
  float* dst = out;
  for (int c = 0; c < chunks; ++c) {
    const int n = base + (c < extra ? 1 : 0);
    processChunk(dst, n);
    dst += 2 * n;
  }

  float blockLoad = 0.f;
  if (cfg_.realtime) {
    const std::uint64_t t1 = clock();
    const double budgetNs = 1e9 * static_cast<double>(numFrames) / cfg_.sampleRate;
    blockLoad = static_cast<float>(static_cast<double>(t1 - t0) / budgetNs);
    loadEma_ = callbacks_ == 0 ? blockLoad : loadEma_ + (blockLoad - loadEma_) * 0.1f;
    loadPeak_ = std::max(blockLoad, loadPeak_ * 0.9995f);
    if (blockLoad > 1.f) ++deadlineMisses_;
    granular_->updateLoad(blockLoad, static_cast<float>(numFrames) / static_cast<float>(cfg_.sampleRate));
  }
  publishStatus(numFrames, loadEma_, blockLoad);
  dj::DjStatus& ds = djBuf_.writeSlot();
  dj_->fillStatus(ds);
  ds.padStep = pads_->currentStep();
  djBuf_.publish();
}

void Engine::processChunk(float* dst, int frames) noexcept {
  const double bpm = params_->get(ids_.bpm);
  bpmShown_ = bpm;
  const BlockTime t = transport_.beginBlock(frames, bpm);
  const std::size_t floats = static_cast<std::size_t>(frames) * 2;
  std::memset(dst, 0, floats * sizeof(float));

  const MasterClock mc = dj_->masterClock(t);
  masterBeat_ = mc.beat;
  masterFromMusic_ = mc.fromMusic;
  if (graph_) {
    for (int i = 0; i < graph_->count; ++i) {
      const GraphDesc::Entry& e = graph_->entries[i];
      e.node->setParams(*params_, t);
      std::memset(nodeBuf_, 0, floats * sizeof(float));
      ProcessIO io;
      io.time = &t;
      io.clock = &mc;
      io.in = nullptr;
      io.out = nodeBuf_;
      io.frames = frames;
      io.garbage = &garbage_;
      e.node->process(io);
      const float g = e.gain;
      for (std::size_t k = 0; k < floats; ++k) dst[k] += nodeBuf_[k] * g;
    }
  }

  // Performance effects sit on the summed bus, in front of the master fader
  fx_->setParams(*params_);
  fx_->process(dst, frames, mc);

  // Master: smoothed gain ramp, safety soft-clip, peak meter.
  const float target = params_->get(ids_.masterGain);
  const bool first = !masterSmooth_.primed;
  const float coef = onePoleBlockCoef(20.f, static_cast<float>(frames / t.sampleRate));
  const float g1 = masterSmooth_.next(target, coef);
  const float g0 = first ? g1 : masterGainPrev_;
  const float dg = (g1 - g0) / static_cast<float>(frames);
  masterGainPrev_ = g1;
  float pl = 0.f, pr = 0.f;
  for (int i = 0; i < frames; ++i) {
    const float g = g0 + dg * static_cast<float>(i);
    const float l = softClip(dst[2 * i] * g);
    const float r = softClip(dst[2 * i + 1] * g);
    dst[2 * i] = l;
    dst[2 * i + 1] = r;
    pl = std::max(pl, std::fabs(l));
    pr = std::max(pr, std::fabs(r));
    capRing_[2 * capPos_].store(l, std::memory_order_relaxed);
    capRing_[2 * capPos_ + 1].store(r, std::memory_order_relaxed);
    capPos_ = capPos_ + 1 == capFrames_ ? 0 : capPos_ + 1;
  }
  capWritten_.fetch_add(static_cast<std::uint64_t>(frames), std::memory_order_release);
  peakL_ = std::max(pl, peakL_);
  peakR_ = std::max(pr, peakR_);

  transport_.endBlock(t);
}

void Engine::publishStatus(int callbackFrames, float load, float blockLoad) noexcept {
  const granular::GranularStats gs = granular_->stats();
  EngineStatus& s = statusBuf_.writeSlot();
  s.load = load;
  s.loadPeak = loadPeak_;
  s.lastBlockLoad = blockLoad;
  s.peakL = peakL_;
  s.peakR = peakR_;
  peakL_ *= 0.985f;  // peak-hold ballistics: the UI polls far slower than the callback rate
  peakR_ *= 0.985f;
  s.deadlineMisses = deadlineMisses_;
  s.xruns = xruns_.load(std::memory_order_relaxed);
  s.droppedGrains = gs.droppedGrains;
  s.callbacks = ++callbacks_;
  s.clockBeat = transport_.clockBeat();
  s.playBeat = transport_.playBeat();
  s.bpm = bpmShown_;
  {
    // where the music is inside its bar: the UI pulses on this (phase inside the beat, beat 0..3 of the bar)
    const double b = masterBeat_ < 0.0 ? 0.0 : masterBeat_;
    const double whole = std::floor(b);
    s.beatPhase = static_cast<float>(b - whole);
    s.beatIndex = static_cast<std::int32_t>(static_cast<std::int64_t>(whole) % 4);
    s.beatFromMusic = masterFromMusic_ ? 1 : 0;
  }
  s.playing = transport_.playing() ? 1 : 0;
  s.activeGrains = gs.activeGrains;
  s.activeVoices = gs.activeVoices;
  s.qualityLevel = gs.qualityLevel;
  s.callbackFrames = callbackFrames;
  s.sampleRate = cfg_.sampleRate;
  statusBuf_.publish();
}

}  // namespace autodj
