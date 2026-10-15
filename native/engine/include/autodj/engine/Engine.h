#pragma once
// Composition root of the native audio core. Platform independent: the Oboe driver (Android) and
// the offline renderer / test harnesses (host) both just call process().
//
//   control thread (UI, MIDI, coroutines)            audio thread (Oboe callback / offline loop)
//   ------------------------------------            --------------------------------------------
//   ParamStore::set()  ---- atomics ---------->      setParams() per block
//   post(Command)      ---- SpscQueue -------->      drainCommands() at callback start
//   setSource(buf)     ---- PublishSource ---->      SourceTable
//   collectGarbage()   <--- GarbageQueue -------      retired buffers / graphs
//   readStatus()/readGrainViz() <-- TripleBuffer      published once per callback / at ~60 Hz
//
// process() is block-size agnostic: any callback size is split into equal chunks of at most
// maxBlockFrames, so burst sizes of 96, 128, 192, 240, 480 ... all work without a FIFO and without
// extra latency.

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "autodj/core/Clock.h"
#include "autodj/core/Command.h"
#include "autodj/core/Dsp.h"
#include "autodj/core/Node.h"
#include "autodj/core/Param.h"
#include "autodj/core/ParamSerializer.h"
#include "autodj/core/SampleBuffer.h"
#include "autodj/core/Status.h"
#include "autodj/core/Transport.h"
#include "autodj/core/TripleBuffer.h"
#include "autodj/dj/DjNode.h"
#include "autodj/dj/DjStatus.h"
#include "autodj/dj/MixPlan.h"
#include "autodj/engine/ToneNode.h"
#include "autodj/engine/Tracks.h"
#include "autodj/granular/GranularNode.h"
#include "autodj/perf/MasterFx.h"
#include "autodj/perf/PadNode.h"
#include "autodj/perf/PerfParams.h"

namespace autodj {

struct EngineConfig {
  int sampleRate = 48000;
  int maxBlockFrames = 192;   // upper bound of one internal processing block (<= kMaxBlockFrames)
  int granularVoices = 4;
  bool realtime = true;       // false = offline render: no timing, no adaptive quality (deterministic)
  ClockFn clock = nullptr;    // injectable for tests, default monotonicNowNs
  std::uint32_t seed = 0xA5F00D01u;
};

struct EngineParamIds {
  ParamId bpm = kInvalidParam;
  ParamId masterGain = kInvalidParam;
  ToneParamIds tone;
  granular::GranularParamIds granular;
  dj::DjParamIds dj;
  perf::PerfParamIds perf;
};

class Engine {
 public:
  explicit Engine(const EngineConfig& cfg = {});
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // ---------------------------------------------------------------------------------------
  // Control side. Safe from any non-audio thread.
  // ---------------------------------------------------------------------------------------
  const ParamRegistry& registry() const { return reg_; }
  ParamStore& params() { return *params_; }
  const EngineParamIds& ids() const { return ids_; }
  int sampleRate() const { return cfg_.sampleRate; }

  bool post(const Command& cmd);  // false if the command queue is full
  bool play();
  bool stop();
  bool seekBeats(double beats);
  bool noteOn(int voice, float pitchSemitones = 0.f, float velocity = 1.f);
  bool noteOff(int voice);
  /// Restarts the Step-mode sequence of a voice at cell 0 (the "reset" button).
  bool resetSteps(int voice);
  bool resetEngine();

  /// Hands a decoded buffer to the audio thread (slot 0..15). The engine keeps it alive until the
  /// audio thread has returned it through the garbage queue.
  bool setSource(int slot, std::shared_ptr<const SampleBuffer> buffer);

  /// Waveform overview (min/max per column) and length of what a source slot currently holds. false if the slot is empty.
  /// Control thread; reads the immutable buffer, never touches the audio thread.
  bool sourcePeaks(int slot, int columns, float* out) const;
  bool sourceInfo(int slot, double* seconds, int* sampleRate, std::string* name) const;

  // ---- tracks (decoded + analysed songs) ------------------------------------------------------
  /// Analyses `audio` (or adopts `cached` when given), builds its peak pyramid and registers it. Returns the track id (> 0).
  /// Control/worker thread; the analysis takes about a second per track, nothing else is blocked meanwhile.
  int addTrack(std::shared_ptr<const SampleBuffer> audio, const std::string& name, const analysis::TrackAnalysis* cached = nullptr,
               const analysis::ProgressFn& progress = {});
  /// Registers a track that has an analysis but no audio yet (a library entry): enough to plan a mix with, see dj::PlanAudio.
  int addTrackStub(const std::string& name, const analysis::TrackAnalysis& analysis);
  std::shared_ptr<const Track> track(int id) const;
  void removeTrack(int id);
  std::size_t trackCount() const;

  // ---- DJ decks and automatic mix ----------------------------------------------------------------
  /// Loads a track into deck 0/1 (null = unload). The engine keeps it alive until the audio thread returns it.
  bool deckLoad(int deck, std::shared_ptr<const Track> track, double startSec = 0.0);
  bool deckPlay(int deck, bool play);
  bool deckSeek(int deck, double seconds);
  /// Plans a mix of the given tracks in this order (unknown ids are skipped). Pure computation, nothing is started.
  std::shared_ptr<const dj::MixPlan> planMix(const std::vector<int>& trackIds, const dj::PlanOptions& options) const;
  /// Suggested play order for the given tracks: the result holds indices into `trackIds` (unknown ids are dropped).
  std::vector<int> orderTracks(const std::vector<int>& trackIds, dj::OrderMode mode) const;
  /// Starts a plan on the decks (stops decks and plans that are running). The engine keeps the plan alive.
  bool planStart(std::shared_ptr<const dj::MixPlan> plan);
  bool planStop();
  bool readDjStatus(dj::DjStatus& out);
  dj::DjNode& djNode() { return *dj_; }
  std::size_t heldTrackCount() const;
  std::size_t heldPlanCount() const;

  // ---- performance: master effects (plain parameters "fx.*") and the drum pads ------------------------
  /// Plays a pad once (velocity 0..1.5, 0 = full).
  bool padTrigger(int pad, float velocity = 0.f);
  /// Sets the 16-bit step mask of a pad's sequencer row (bit s = step s).
  bool padSetRow(int pad, unsigned mask);

  // ---- granular step pattern ------------------------------------------------------------------------------
  /// Sets the 16-step pattern of a granular voice: bit s of `mask` = step s plays, `cells` holds the cell of step s in bits 4s..4s+3.
  /// It is used while the voice is in Step mode with its "seq_on" parameter set.
  bool granularSetSteps(int voice, unsigned mask, std::uint64_t cells);

  /// Copies the last `seconds` (at most kCaptureSeconds) of what the engine has sent to the speakers into a new buffer, so that it
  /// can be played back by the granular instrument ("freeze"). Control thread; null while nothing has been rendered yet.
  std::shared_ptr<const SampleBuffer> captureOutput(double seconds, const std::string& name) const;
  static constexpr double kCaptureSeconds = 8.0;

  /// Replaces the processing graph (pointer swap on the audio thread). Previous graph is freed here.
  bool publishGraph(std::unique_ptr<GraphDesc> graph);

  /// Builds a graph out of this engine's own nodes (tone + granular) with the given mix gains.
  std::unique_ptr<GraphDesc> makeGraph(float toneGain, float granularGain);

  /// Frees objects the audio thread has retired. Call periodically (e.g. from the UI poll timer).
  std::size_t collectGarbage();

  /// Number of buffers / graphs the control side still keeps alive for the audio thread (leak checks).
  std::size_t heldBufferCount() const;
  std::size_t heldGraphCount() const;

  bool readStatus(EngineStatus& out);
  bool readGrainViz(GrainViz& out);
  /// Driver-reported underruns (Oboe). Written from the audio callback, shown in the debug overlay.
  void reportXruns(std::uint32_t total) noexcept { xruns_.store(total, std::memory_order_relaxed); }

  std::string exportPreset(const std::string& name) const;
  LoadReport importPreset(const std::string& json);

  granular::GranularNode& granularNode() { return *granular_; }
  perf::PadNode& padNode() { return *pads_; }

  // ---------------------------------------------------------------------------------------
  // Audio side.
  // ---------------------------------------------------------------------------------------
  /// Renders `numFrames` interleaved stereo float frames. Never allocates, locks or throws.
  void process(float* out, int numFrames) noexcept;

 private:
  void drainCommands() noexcept;
  void applyCommand(const Command& cmd) noexcept;
  void processChunk(float* dst, int frames) noexcept;
  void publishStatus(int callbackFrames, float load, float blockLoad) noexcept;

  EngineConfig cfg_;
  ParamRegistry reg_;
  EngineParamIds ids_;
  std::unique_ptr<ParamStore> params_;

  CommandQueue commands_;
  GarbageQueue garbage_;
  std::mutex postMutex_;  // serialises producers; never taken on the audio thread

  Transport transport_;
  std::unique_ptr<ToneNode> tone_;
  std::unique_ptr<granular::GranularNode> granular_;
  std::unique_ptr<dj::DjNode> dj_;
  std::unique_ptr<perf::PadNode> pads_;
  std::unique_ptr<perf::MasterFx> fx_;
  Node* allNodes_[4] = {};
  const GraphDesc* graph_ = nullptr;

  mutable std::mutex ownerMutex_;  // control-side ownership tables
  std::unordered_map<const SampleBuffer*, std::shared_ptr<const SampleBuffer>> heldBuffers_;
  std::shared_ptr<const SampleBuffer> slotBuffers_[16];  // latest buffer per slot, for the UI overview only
  std::unordered_map<const GraphDesc*, std::unique_ptr<GraphDesc>> heldGraphs_;
  struct HeldTrack {
    std::shared_ptr<const Track> track;
    int refs = 0;  // loads that the audio thread has not handed back yet
  };
  std::unordered_map<const Track*, HeldTrack> heldTracks_;
  std::unordered_map<const dj::MixPlan*, std::shared_ptr<const dj::MixPlan>> heldPlans_;
  std::uint32_t nextPlanId_ = 1;
  std::uint32_t graphVersion_ = 0;
  mutable std::mutex trackMutex_;
  std::unordered_map<int, std::shared_ptr<const Track>> tracks_;
  int nextTrackId_ = 1;

  // the last seconds of output, written by the audio thread (relaxed atomics: the control side copies from the part that is not
  // being overwritten, see captureOutput)
  std::unique_ptr<std::atomic<float>[]> capRing_;
  std::size_t capFrames_ = 0;
  std::size_t capPos_ = 0;
  std::atomic<std::uint64_t> capWritten_{0};

  TripleBuffer<EngineStatus> statusBuf_;
  TripleBuffer<GrainViz> vizBuf_;
  TripleBuffer<dj::DjStatus> djBuf_;
  std::mutex readMutex_;

  // audio-thread state
  alignas(kCacheLine) float nodeBuf_[kMaxBlockFrames * 2];
  BlockSmoother masterSmooth_;
  float masterGainPrev_ = 0.f;
  float peakL_ = 0.f, peakR_ = 0.f;
  float loadEma_ = 0.f, loadPeak_ = 0.f;
  std::uint32_t deadlineMisses_ = 0;
  std::uint64_t callbacks_ = 0;
  double bpmShown_ = 120.0;
  double masterBeat_ = 0.0;      // MasterClock beat at the start of the last block (for the UI pulse)
  bool masterFromMusic_ = false;
  std::atomic<std::uint32_t> xruns_{0};
};

}  // namespace autodj
