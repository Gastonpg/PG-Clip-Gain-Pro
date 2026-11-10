#pragma once

#include "pgclip/core/capture_accumulator.hpp"
#include "pgclip/core/spsc_audio_block_queue.hpp"

#include <cstddef>
#include <vector>

namespace pgclip {

enum class QueueDrainStatus {
    BlockAccepted,
    QueueEmpty,
    CaptureComplete,
    CaptureRejected,
    QueueError
};

// Worker-side bridge from the realtime SPSC queue into the bounded capture.
// Construct and call this only from the worker/control side, never the callback.
class QueuedCaptureAssembler {
public:
    QueuedCaptureAssembler(AudioFormat format,
                           FrameIndex captureCapacityFrames,
                           std::uint32_t maxFramesPerBlock);

    QueuedCaptureAssembler(const QueuedCaptureAssembler&) = delete;
    QueuedCaptureAssembler& operator=(const QueuedCaptureAssembler&) = delete;

    bool begin(SpscAudioBlockQueue& queue,
               FrameIndex sourceStartFrame,
               FrameIndex expectedFrameCount) noexcept;
    QueueDrainStatus drainOne(SpscAudioBlockQueue& queue) noexcept;
    void finish(SpscAudioBlockQueue& queue) noexcept;
    void finishAtCapturedLength(SpscAudioBlockQueue& queue) noexcept;
    void cancel() noexcept;

    CaptureDescriptor descriptor() const noexcept;
    const float* data() const noexcept;
    std::size_t sampleCount() const noexcept;
    FrameIndex capturedFrames() const noexcept;

private:
    AudioFormat format_{};
    FrameIndex expectedFrameCount_{0};
    std::uint64_t droppedBlocksAtBegin_{0};
    CaptureAccumulator accumulator_;
    std::vector<float> scratch_;
};

} // namespace pgclip
