#include "pgclip/core/gain_curve_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pgclip {

const std::vector<LinearGainPoint>&
GainCurveRuntime::points() const noexcept {
    return points_;
}

GainCurveRuntimeResult prepareGainCurveRuntime(
    const GainCurve& curve,
    const GainCurveRuntimeConfig& config) {
    GainCurveRuntimeResult result;

    if (!std::isfinite(config.maximumBoostDb) ||
        config.maximumBoostDb < 0.0 ||
        !std::isfinite(config.maximumCutDb) ||
        config.maximumCutDb < 0.0) {
        result.status = GainCurveRuntimeStatus::InvalidCurve;
        result.issue = {ValidationCode::InvalidWindow,
                        "Runtime gain safety limits must be finite and non-negative."};
        return result;
    }

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
        if (point.gainDb > config.maximumBoostDb ||
            point.gainDb < -config.maximumCutDb) {
            result.runtime.points_.clear();
            result.status = GainCurveRuntimeStatus::InvalidCurve;
            result.issue = {ValidationCode::NonFiniteGain,
                            "Runtime curve exceeds the configured combined gain safety limits."};
            return result;
        }

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

double evaluateCurveLinear(const GainCurve& curve, FrameIndex frame) {
    if (curve.points.empty() || frame < curve.points.front().frame ||
        frame > curve.points.back().frame) {
        return 1.0;
    }

    std::size_t segment = 0U;
    while (segment + 1U < curve.points.size() &&
           frame > curve.points[segment + 1U].frame) {
        ++segment;
    }

    if (segment + 1U >= curve.points.size()) {
        return gainDbToLinear(curve.points.back().gainDb);
    }

    const auto& start = curve.points[segment];
    const auto& end = curve.points[segment + 1U];
    const double position =
        static_cast<double>(frame - start.frame) /
        static_cast<double>(end.frame - start.frame);
    const double smooth = position * position *
                          (3.0 - 2.0 * position);
    const double startGain = gainDbToLinear(start.gainDb);
    const double endGain = gainDbToLinear(end.gainDb);
    return startGain + (endGain - startGain) * smooth;
}

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

CurveCombinationResult combineStaticGainCurves(
    const GainCurve& clipGainCurve,
    const GainCurve& clipPeakCurve,
    FrameIndex totalFrames,
    const CurveCombinationConfig& config) {
    CurveCombinationResult result;

    if (totalFrames == 0U ||
        !std::isfinite(config.maximumCombinedBoostDb) ||
        config.maximumCombinedBoostDb < 0.0 ||
        !std::isfinite(config.maximumCombinedCutDb) ||
        config.maximumCombinedCutDb < 0.0) {
        result.status = CurveCombinationStatus::InvalidConfiguration;
        result.issue = {ValidationCode::InvalidWindow,
                        "Combined curve limits and capture length are invalid."};
        return result;
    }

    const auto validateInput = [totalFrames](const GainCurve& curve) {
        if (const auto issue = validate(curve)) {
            return issue;
        }
        if (!curve.points.empty() && curve.points.size() < 2U) {
            return ValidationIssue{
                ValidationCode::UnorderedCurvePoints,
                "A non-empty source curve requires at least two points."
            };
        }
        for (const auto& point : curve.points) {
            if (point.frame > totalFrames) {
                return ValidationIssue{
                    ValidationCode::RangeOutsideCapture,
                    "A source curve point extends beyond the capture timeline."
                };
            }
        }
        return ValidationIssue{};
    };

    if (const auto issue = validateInput(clipGainCurve)) {
        result.status = CurveCombinationStatus::InvalidInputCurve;
        result.issue = issue;
        return result;
    }
    if (const auto issue = validateInput(clipPeakCurve)) {
        result.status = CurveCombinationStatus::InvalidInputCurve;
        result.issue = issue;
        return result;
    }

    std::vector<FrameIndex> frames;
    frames.reserve(clipGainCurve.points.size() +
                   clipPeakCurve.points.size());
    for (const auto& point : clipGainCurve.points) {
        frames.push_back(point.frame);
    }
    for (const auto& point : clipPeakCurve.points) {
        frames.push_back(point.frame);
    }

    std::sort(frames.begin(), frames.end());
    frames.erase(std::unique(frames.begin(), frames.end()), frames.end());

    std::vector<FrameIndex> sampledFrames;
    sampledFrames.reserve(frames.size() * 2U);
    for (std::size_t index = 0U; index < frames.size(); ++index) {
        sampledFrames.push_back(frames[index]);
        if (index + 1U < frames.size()) {
            const FrameIndex span = frames[index + 1U] - frames[index];
            const FrameIndex midpoint = frames[index] + span / 2U;
            if (midpoint > frames[index] &&
                midpoint < frames[index + 1U]) {
                sampledFrames.push_back(midpoint);
            }
        }
    }

    result.curve.points.reserve(sampledFrames.size());
    for (const FrameIndex frame : sampledFrames) {
        const double clipGain = evaluateCurveLinear(clipGainCurve, frame);
        const double clipPeak = evaluateCurveLinear(clipPeakCurve, frame);
        const double combinedLinear = clipGain * clipPeak;

        if (!std::isfinite(combinedLinear) || combinedLinear <= 0.0) {
            result.curve.points.clear();
            result.status = CurveCombinationStatus::InvalidInputCurve;
            result.issue = {ValidationCode::NonFiniteGain,
                            "Combined linear gain is not representable."};
            return result;
        }

        const double combinedDb = std::clamp(
            20.0 * std::log10(combinedLinear),
            -config.maximumCombinedCutDb,
            config.maximumCombinedBoostDb);
        result.curve.points.push_back({frame, combinedDb});
    }

    if (const auto issue = validate(result.curve)) {
        result.curve.points.clear();
        result.status = CurveCombinationStatus::InvalidInputCurve;
        result.issue = issue;
        return result;
    }

    result.status = CurveCombinationStatus::Ready;
    return result;
}

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
