#pragma once

#include "pgclip/core/spsc_audio_block_queue.hpp"
#include "pgclip/core/types.hpp"

#include <vector>

namespace pgclip {

struct LinearGainPoint {
    FrameIndex frame{0};
    double gainLinear{1.0};
};

class GainCurveRuntime {
public:
    const std::vector<LinearGainPoint>& points() const noexcept;

private:
    friend struct GainCurveRuntimeResult;
    friend GainCurveRuntimeResult prepareGainCurveRuntime(const GainCurve&);
    std::vector<LinearGainPoint> points_;
};

enum class GainCurveRuntimeStatus {
    Ready,
    InvalidCurve
};

struct GainCurveRuntimeResult {
    GainCurveRuntimeStatus status{GainCurveRuntimeStatus::InvalidCurve};
    ValidationIssue issue{};
    GainCurveRuntime runtime{};
};

GainCurveRuntimeResult prepareGainCurveRuntime(const GainCurve& curve);

struct MutableAudioBlockView {
    void* const* channelData{nullptr};
    FrameIndex sourceStartFrame{0};
    std::uint32_t frameCount{0};
    std::uint32_t channelCount{0};
    AudioSampleFormat sampleFormat{AudioSampleFormat::Float32};
};

enum class GainCurveApplyStatus {
    Applied,
    NoCurve,
    InvalidBlock,
    InvalidRuntime
};

// Realtime-safe application: no allocations, locks, file access or dynamic
// processing. Uses smoothstep interpolation between precomputed linear gains.
GainCurveApplyStatus applyGainCurveToBlock(
    const MutableAudioBlockView& block,
    const GainCurveRuntime& runtime) noexcept;

} // namespace pgclip
