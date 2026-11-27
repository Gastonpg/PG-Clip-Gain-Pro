#pragma once

#include "pgclip/core/capture_accumulator.hpp"
#include "pgclip/core/spsc_audio_block_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pgclip {

enum class QueueDrainStatus {
    BlockAccepted,
    QueueEmpty,
    CaptureComplete,
    CaptureRejected,
    QueueError
};

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

    // Worker-side begin after the callback has started producing. The baseline
    // preserves overflow detection even if blocks were queued before begin.
    bool beginFromActiveProducer(SpscAudioBlockQueue& queue,
                                 FrameIndex sourceStartFrame,
                                 FrameIndex expectedFrameCount,
                                 std::uint64_t droppedBlocksBaseline) noexcept;

    QueueDrainStatus drainOne(SpscAudioBlockQueue& queue) noexcept;
    bool finish(SpscAudioBlockQueue& queue) noexcept;
    bool finishAtCapturedLength(SpscAudioBlockQueue& queue) noexcept;
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
