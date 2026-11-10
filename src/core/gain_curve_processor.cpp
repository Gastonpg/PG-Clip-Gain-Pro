#include "pgclip/core/gain_curve_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pgclip {

const std::vector<LinearGainPoint>&
GainCurveRuntime::points() const noexcept {
    return points_;
}

GainCurveRuntimeResult prepareGainCurveRuntime(const GainCurve& curve) {
    GainCurveRuntimeResult result;

    if (const auto issue = validate(curve)) {
        result.status = GainCurveRuntimeStatus::InvalidCurve;
        result.issue = issue;
        return result;
    }

    if (!curve.points.empty() && curve.points.size() < 2U) {
        result.status = GainCurveRuntimeStatus::InvalidCurve;
        result.issue = {ValidationCode::UnorderedCurvePoints,
                        "A runtime gain curve requires at least two points."};
        return result;
    }

    result.runtime.points_.reserve(curve.points.size());
    for (const auto& point : curve.points) {
        const double linearGain = gainDbToLinear(point.gainDb);
        if (!std::isfinite(linearGain) || linearGain < 0.0) {
            result.runtime.points_.clear();
            result.status = GainCurveRuntimeStatus::InvalidCurve;
            result.issue = {ValidationCode::NonFiniteGain,
                            "Gain curve conversion produced a non-finite linear value."};
            return result;
        }
        result.runtime.points_.push_back({point.frame, linearGain});
    }

    result.status = GainCurveRuntimeStatus::Ready;
    return result;
}

namespace {

bool validBlock(const MutableAudioBlockView& block) noexcept {
    if (block.frameCount == 0U) {
        return true;
    }

    if (block.channelData == nullptr ||
        (block.channelCount != 1U && block.channelCount != 2U)) {
        return false;
    }

    const auto finalOffset =
        static_cast<FrameIndex>(block.frameCount - 1U);
    if (finalOffset >
        std::numeric_limits<FrameIndex>::max() - block.sourceStartFrame) {
        return false;
    }

    if (block.sampleFormat != AudioSampleFormat::Float32 &&
        block.sampleFormat != AudioSampleFormat::Float64) {
        return false;
    }

    for (std::uint32_t channel = 0U;
         channel < block.channelCount; ++channel) {
        if (block.channelData[channel] == nullptr) {
            return false;
        }
    }

    return true;
}

double gainAtFrame(const std::vector<LinearGainPoint>& points,
                   FrameIndex frame,
                   std::size_t& segment) noexcept {
    if (points.empty() || frame < points.front().frame ||
        frame > points.back().frame) {
        return 1.0;
    }

    while (segment + 1U < points.size() &&
           frame > points[segment + 1U].frame) {
        ++segment;
    }

    if (segment + 1U >= points.size()) {
        return points.back().gainLinear;
    }

    const auto& start = points[segment];
    const auto& end = points[segment + 1U];
    const FrameIndex span = end.frame - start.frame;
    if (span == 0U) {
        return end.gainLinear;
    }

    const double position =
        static_cast<double>(frame - start.frame) /
        static_cast<double>(span);
    const double smooth = position * position *
                          (3.0 - 2.0 * position);

    return start.gainLinear +
           (end.gainLinear - start.gainLinear) * smooth;
}

template <typename Sample>
void applyTyped(const MutableAudioBlockView& block,
                const std::vector<LinearGainPoint>& points) noexcept {
    std::size_t segment = 0U;

    for (std::uint32_t frame = 0U; frame < block.frameCount; ++frame) {
        const FrameIndex absoluteFrame =
            block.sourceStartFrame + static_cast<FrameIndex>(frame);
        const double gain = gainAtFrame(points, absoluteFrame, segment);

        for (std::uint32_t channel = 0U;
             channel < block.channelCount; ++channel) {
            auto* samples =
                static_cast<Sample*>(block.channelData[channel]);
            samples[frame] = static_cast<Sample>(
                static_cast<double>(samples[frame]) * gain);
        }
    }
}

} // namespace

GainCurveApplyStatus applyGainCurveToBlock(
    const MutableAudioBlockView& block,
    const GainCurveRuntime& runtime) noexcept {
    if (!validBlock(block)) {
        return GainCurveApplyStatus::InvalidBlock;
    }

    const auto& points = runtime.points();
    if (points.empty()) {
        return GainCurveApplyStatus::NoCurve;
    }
    if (points.size() < 2U) {
        return GainCurveApplyStatus::InvalidRuntime;
    }

    if (block.frameCount == 0U) {
        return GainCurveApplyStatus::Applied;
    }

    if (block.sampleFormat == AudioSampleFormat::Float32) {
        applyTyped<float>(block, points);
    } else if (block.sampleFormat == AudioSampleFormat::Float64) {
        applyTyped<double>(block, points);
    } else {
        return GainCurveApplyStatus::InvalidBlock;
    }

    return GainCurveApplyStatus::Applied;
}

} // namespace pgclip
