#pragma once

#include "pgclip/core/types.hpp"

#include <cstddef>
#include <vector>

namespace pgclip {

enum class CaptureAppendResult {
    Accepted,
    NotCapturing,
    InvalidBlock,
    GapDetected,
    OverlapDetected,
    CapacityExceeded
};

// Single-producer, preallocated capture storage.
// Construct and begin outside the audio callback; append performs no allocation.
class CaptureAccumulator {
public:
    CaptureAccumulator(AudioFormat format, FrameIndex capacityFrames);

    void begin(FrameIndex sourceStartFrame) noexcept;
    CaptureAppendResult append(FrameIndex sourceStartFrame,
                               const float* interleaved,
                               FrameIndex frameCount) noexcept;
    void finish() noexcept;
    void cancel() noexcept;

    CaptureDescriptor descriptor() const noexcept;
    const float* data() const noexcept;
    std::size_t sampleCount() const noexcept;
    FrameIndex capturedFrames() const noexcept;

private:
    AudioFormat format_{};
    FrameIndex capacityFrames_{0};
    FrameIndex capturedFrames_{0};
    FrameIndex expectedSourceFrame_{0};
    CaptureState state_{CaptureState::Empty};
    bool hasDiscontinuities_{false};
    std::vector<float> storage_;
};

} // namespace pgclip
