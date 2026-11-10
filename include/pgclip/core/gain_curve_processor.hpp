#pragma once

#include "pgclip/core/spsc_audio_block_queue.hpp"
#include "pgclip/core/types.hpp"

#include <utility>
#include <vector>

namespace pgclip {

struct LinearGainPoint {
    FrameIndex frame{0};
    double gainLinear{1.0};
};

struct GainCurveRuntimeResult;
struct GainCurveRuntimeConfig;

class GainCurveRuntime {
public:
    GainCurveRuntime() = default;
    explicit GainCurveRuntime(std::vector<LinearGainPoint> points)
        : points_(std::move(points)) {}

    const std::vector<LinearGainPoint>& points() const noexcept;

private:
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

struct GainCurveRuntimeConfig {
    double maximumBoostDb{0.0};
    double maximumCutDb{0.0};
};

GainCurveRuntimeResult prepareGainCurveRuntime(
    const GainCurve& curve,
    const GainCurveRuntimeConfig& config);

struct CurveCombinationConfig {
    double maximumCombinedBoostDb{0.0};
    double maximumCombinedCutDb{0.0};
};

enum class CurveCombinationStatus {
    Ready,
    InvalidConfiguration,
    InvalidInputCurve
};

struct CurveCombinationResult {
    CurveCombinationStatus status{CurveCombinationStatus::InvalidConfiguration};
    ValidationIssue issue{};
    GainCurve curve{};
};

// Combines independent static engines in the linear-gain domain and applies
// a shared safety ceiling to the total correction.
CurveCombinationResult combineStaticGainCurves(
    const GainCurve& clipGainCurve,
    const GainCurve& clipPeakCurve,
    FrameIndex totalFrames,
    const CurveCombinationConfig& config);

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
