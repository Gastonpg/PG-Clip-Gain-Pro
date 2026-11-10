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

// Preallocated bounded capture storage.
// All lifecycle calls must be serialized with append; begin/finish/cancel are
// called only while the host callback is quiescent. append performs no allocation.
class CaptureAccumulator {
public:
    CaptureAccumulator(AudioFormat format, FrameIndex capacityFrames);

    bool begin(FrameIndex sourceStartFrame,
               FrameIndex expectedFrameCount) noexcept;
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
    FrameIndex expectedFrameCount_{0};
    FrameIndex expectedSourceFrame_{0};
    CaptureState state_{CaptureState::Empty};
    bool hasDiscontinuities_{false};
    std::vector<float> storage_;
};

} // namespace pgclip
