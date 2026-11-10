#include "pgclip/core/capture_accumulator.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgclip {

CaptureAccumulator::CaptureAccumulator(AudioFormat format,
                                       FrameIndex capacityFrames)
    : format_(format), capacityFrames_(capacityFrames) {
    if (const auto issue = validate(format_)) {
        throw std::invalid_argument(issue.message);
    }

    const auto channels = static_cast<std::size_t>(format_.channelCount);
    const auto maxFrames = std::vector<float>{}.max_size() / channels;
    if (capacityFrames_ == 0U ||
        capacityFrames_ > static_cast<FrameIndex>(maxFrames)) {
        throw std::invalid_argument("Capture capacity is zero or exceeds addressable storage.");
    }

    storage_.resize(static_cast<std::size_t>(capacityFrames_) * channels);
}

void CaptureAccumulator::begin(FrameIndex sourceStartFrame) noexcept {
    capturedFrames_ = 0U;
    expectedSourceFrame_ = sourceStartFrame;
    hasDiscontinuities_ = false;
    state_ = CaptureState::Capturing;
}

CaptureAppendResult CaptureAccumulator::append(
    FrameIndex sourceStartFrame,
    const float* interleaved,
    FrameIndex frameCount) noexcept {
    if (state_ != CaptureState::Capturing) {
        return CaptureAppendResult::NotCapturing;
    }

    if (interleaved == nullptr || frameCount == 0U ||
        frameCount > std::numeric_limits<FrameIndex>::max() - sourceStartFrame) {
        return CaptureAppendResult::InvalidBlock;
    }

    if (sourceStartFrame > expectedSourceFrame_) {
        hasDiscontinuities_ = true;
        state_ = CaptureState::Discontinuous;
        return CaptureAppendResult::GapDetected;
    }

    if (sourceStartFrame < expectedSourceFrame_) {
        hasDiscontinuities_ = true;
        state_ = CaptureState::Discontinuous;
        return CaptureAppendResult::OverlapDetected;
    }

    if (frameCount > capacityFrames_ - capturedFrames_) {
        state_ = CaptureState::Partial;
        return CaptureAppendResult::CapacityExceeded;
    }

    const auto channels = static_cast<std::size_t>(format_.channelCount);
    const auto destinationOffset =
        static_cast<std::size_t>(capturedFrames_) * channels;
    const auto blockSamples = static_cast<std::size_t>(frameCount) * channels;

    std::copy_n(interleaved, blockSamples,
                storage_.data() + destinationOffset);

    capturedFrames_ += frameCount;
    expectedSourceFrame_ += frameCount;
    return CaptureAppendResult::Accepted;
}

void CaptureAccumulator::finish() noexcept {
    if (state_ == CaptureState::Capturing) {
        state_ = capturedFrames_ > 0U
            ? CaptureState::Complete
            : CaptureState::Partial;
    }
}

void CaptureAccumulator::cancel() noexcept {
    if (state_ == CaptureState::Capturing) {
        state_ = CaptureState::Cancelled;
    }
}

CaptureDescriptor CaptureAccumulator::descriptor() const noexcept {
    return {format_, capturedFrames_, state_, hasDiscontinuities_};
}

const float* CaptureAccumulator::data() const noexcept {
    return storage_.data();
}

std::size_t CaptureAccumulator::sampleCount() const noexcept {
    return static_cast<std::size_t>(capturedFrames_) *
           static_cast<std::size_t>(format_.channelCount);
}

FrameIndex CaptureAccumulator::capturedFrames() const noexcept {
    return capturedFrames_;
}

} // namespace pgclip
