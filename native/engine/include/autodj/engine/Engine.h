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
#include "autodj/engine/ToneNode.h"
#include "autodj/granular/GranularNode.h"

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
  Node* allNodes_[2] = {};
  const GraphDesc* graph_ = nullptr;

  mutable std::mutex ownerMutex_;  // control-side ownership tables
  std::unordered_map<const SampleBuffer*, std::shared_ptr<const SampleBuffer>> heldBuffers_;
  std::shared_ptr<const SampleBuffer> slotBuffers_[16];  // latest buffer per slot, for the UI overview only
  std::unordered_map<const GraphDesc*, std::unique_ptr<GraphDesc>> heldGraphs_;
  std::uint32_t graphVersion_ = 0;

  TripleBuffer<EngineStatus> statusBuf_;
  TripleBuffer<GrainViz> vizBuf_;
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
  std::atomic<std::uint32_t> xruns_{0};
};

}  // namespace autodj
