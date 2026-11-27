#pragma once

#include "pgclip/core/analysis_pipeline.hpp"
#include "pgclip/core/capture_session_control.hpp"
#include "pgclip/core/queued_capture_assembler.hpp"
#include "pgclip/core/shared_analysis_snapshot.hpp"
#include "pgclip/core/transport_continuity_tracker.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace pgclip {

class Vst3CaptureRuntime {
public:
    Vst3CaptureRuntime() = default;
    ~Vst3CaptureRuntime();

    Vst3CaptureRuntime(const Vst3CaptureRuntime&) = delete;
    Vst3CaptureRuntime& operator=(const Vst3CaptureRuntime&) = delete;

    bool start() noexcept;
    void shutdown() noexcept;

    bool requestArm(std::uint32_t sampleRate,
                    std::uint32_t channelCount) noexcept;
    void requestStop() noexcept;

    // Called once per VST3 process block. This function is realtime-safe:
    // no allocation, locks, waits, logging, or file access.
    void captureBlock(const void* const* channelData,
                      std::uint32_t channelCount,
                      std::uint32_t frameCount,
                      std::int64_t projectTimeSamples,
                      bool playing,
                      bool cycleActive,
                      AudioSampleFormat sampleFormat,
                      std::uint64_t silenceFlags) noexcept;

    CaptureSessionState state() const noexcept;
    std::uint64_t lastCapturedFrames() const noexcept;
    bool lastCaptureValid() const noexcept;
    std::shared_ptr<const SharedAnalysisSnapshot> latestAnalysisSnapshot() const noexcept;
    std::shared_ptr<const AnalysisPipelineResult> latestAnalysisResult() const noexcept;

private:
    void workerLoop() noexcept;
    void finishRejectedSession() noexcept;

    std::unique_ptr<SpscAudioBlockQueue> queue_;
    std::thread worker_;
    CaptureSessionControl session_;
    TransportContinuityTracker continuity_;
    std::unique_ptr<QueuedCaptureAssembler> assembler_;
    std::shared_ptr<const SharedAnalysisSnapshot> analysisSnapshot_;
    std::shared_ptr<const AnalysisPipelineResult> analysisResult_;
    std::vector<float> discardScratch_;

    std::atomic<bool> shutdownRequested_{false};
    std::atomic<std::uint32_t> callbacksInFlight_{0};
    std::atomic<std::uint32_t> requestedSampleRate_{0};
    std::atomic<std::uint32_t> requestedChannels_{0};
    std::atomic<std::int64_t> firstProjectSample_{0};
    std::atomic<bool> firstBlockReady_{false};
    std::atomic<std::uint64_t> droppedBlocksAtArm_{0};
    std::atomic<std::uint64_t> lastCapturedFrames_{0};
    std::atomic<bool> lastCaptureValid_{false};
    bool assemblerStarted_{false};
};

} // namespace pgclip
