#include "pgclip/core/queued_capture_assembler.hpp"

#include <limits>
#include <stdexcept>

namespace pgclip {

QueuedCaptureAssembler::QueuedCaptureAssembler(
    AudioFormat format,
    FrameIndex captureCapacityFrames,
    std::uint32_t maxFramesPerBlock)
    : format_(format),
      accumulator_(format, captureCapacityFrames) {
    if (maxFramesPerBlock == 0U) {
        throw std::invalid_argument("Maximum queue block size must be non-zero.");
    }

    const auto channels = static_cast<std::size_t>(format.channelCount);
    const auto frames = static_cast<std::size_t>(maxFramesPerBlock);
    if (frames > std::numeric_limits<std::size_t>::max() / channels) {
        throw std::invalid_argument("Queue scratch buffer size overflows addressable storage.");
    }

    scratch_.resize(frames * channels);
}

bool QueuedCaptureAssembler::begin(
    SpscAudioBlockQueue& queue,
    FrameIndex sourceStartFrame,
    FrameIndex expectedFrameCount) noexcept {
    expectedFrameCount_ = expectedFrameCount;
    droppedBlocksAtBegin_ = queue.droppedBlocks();
    return accumulator_.begin(sourceStartFrame, expectedFrameCount);
}

QueueDrainStatus QueuedCaptureAssembler::drainOne(
    SpscAudioBlockQueue& queue) noexcept {
    if (accumulator_.descriptor().state != CaptureState::Capturing) {
        return QueueDrainStatus::CaptureRejected;
    }

    if (queue.droppedBlocks() != droppedBlocksAtBegin_) {
        accumulator_.markDiscontinuous();
        return QueueDrainStatus::CaptureRejected;
    }

    QueuedAudioBlockInfo info{};
    const auto queueResult =
        queue.tryPop(scratch_.data(), scratch_.size(), info);

    if (queueResult == AudioQueueResult::Empty) {
        return QueueDrainStatus::QueueEmpty;
    }
    if (queueResult != AudioQueueResult::Success) {
        accumulator_.finish();
        return QueueDrainStatus::QueueError;
    }

    if (info.channelCount != format_.channelCount ||
        info.frameCount == 0U) {
        accumulator_.cancel();
        return QueueDrainStatus::CaptureRejected;
    }

    const auto appendResult = accumulator_.append(
        info.sourceStartFrame, scratch_.data(), info.frameCount);

    if (appendResult != CaptureAppendResult::Accepted) {
        accumulator_.finish();
        return QueueDrainStatus::CaptureRejected;
    }

    if (accumulator_.capturedFrames() == expectedFrameCount_) {
        accumulator_.finish();
        return QueueDrainStatus::CaptureComplete;
    }

    return QueueDrainStatus::BlockAccepted;
}

void QueuedCaptureAssembler::finish(SpscAudioBlockQueue& queue) noexcept {
    if (queue.droppedBlocks() != droppedBlocksAtBegin_) {
        accumulator_.markDiscontinuous();
    }
    accumulator_.finish();
}

void QueuedCaptureAssembler::finishAtCapturedLength(
    SpscAudioBlockQueue& queue) noexcept {
    if (queue.droppedBlocks() != droppedBlocksAtBegin_) {
        accumulator_.markDiscontinuous();
    }
    accumulator_.finishAtCapturedLength();
}

void QueuedCaptureAssembler::cancel() noexcept {
    accumulator_.cancel();
}

CaptureDescriptor QueuedCaptureAssembler::descriptor() const noexcept {
    return accumulator_.descriptor();
}

const float* QueuedCaptureAssembler::data() const noexcept {
    return accumulator_.data();
}

std::size_t QueuedCaptureAssembler::sampleCount() const noexcept {
    return accumulator_.sampleCount();
}

FrameIndex QueuedCaptureAssembler::capturedFrames() const noexcept {
    return accumulator_.capturedFrames();
}

} // namespace pgclip
