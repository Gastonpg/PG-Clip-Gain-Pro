#include "pgclip/core/types.hpp"

#include <cmath>

namespace pgclip {

ValidationIssue validate(const AudioFormat& format) {
    if (format.sampleRate < 8000U || format.sampleRate > 384000U) {
        return {ValidationCode::InvalidSampleRate,
                "Sample rate must be between 8000 and 384000 Hz."};
    }

    if (format.channelCount != 1U && format.channelCount != 2U) {
        return {ValidationCode::UnsupportedChannelCount,
                "The initial core supports mono and stereo only."};
    }

    return {};
}

ValidationIssue validate(const FrameRange& range,
                         FrameIndex totalFrames) {
    if (range.start >= range.end) {
        return {ValidationCode::EmptyRange,
                "Frame ranges are start-inclusive and end-exclusive; start must be less than end."};
    }

    if (range.end > totalFrames) {
        return {ValidationCode::RangeOutsideCapture,
                "Frame range extends beyond the captured frame count."};
    }

    return {};
}

ValidationIssue validate(const CaptureDescriptor& capture) {
    if (const auto formatIssue = validate(capture.format)) {
        return formatIssue;
    }

    if (capture.totalFrames == 0U &&
        capture.state != CaptureState::Empty &&
        capture.state != CaptureState::Capturing) {
        return {ValidationCode::EmptyRange,
                "A finalized capture must contain at least one frame."};
    }

    if (capture.state == CaptureState::Complete &&
        capture.hasDiscontinuities) {
        return {ValidationCode::RangeOutsideCapture,
                "A capture with discontinuities cannot be marked complete."};
    }

    if (capture.state == CaptureState::Discontinuous &&
        !capture.hasDiscontinuities) {
        return {ValidationCode::RangeOutsideCapture,
                "Discontinuous state requires an explicit discontinuity flag."};
    }

    return {};
}

ValidationIssue validate(const AnalysisConfig& config) {
    if (config.windowFrames == 0U) {
        return {ValidationCode::InvalidWindow,
                "Analysis window length must be greater than zero."};
    }

    if (config.hopFrames == 0U ||
        config.hopFrames > config.windowFrames) {
        return {ValidationCode::InvalidHop,
                "Analysis hop must be greater than zero and no larger than the window."};
    }

    if (config.version == 0U) {
        return {ValidationCode::InvalidVersion,
                "Analysis configuration version must be non-zero."};
    }

    return {};
}

ValidationIssue validate(const GainCurve& curve) {
    for (std::size_t index = 0; index < curve.points.size(); ++index) {
        const auto& point = curve.points[index];

        if (!std::isfinite(point.gainDb)) {
            return {ValidationCode::NonFiniteGain,
                    "Gain curve contains a non-finite gain value."};
        }

        if (index == 0U) {
            continue;
        }

        if (point.frame < curve.points[index - 1U].frame) {
            return {ValidationCode::UnorderedCurvePoints,
                    "Gain curve points must be ordered by frame."};
        }

        if (point.frame == curve.points[index - 1U].frame) {
            return {ValidationCode::DuplicateCurvePoint,
                    "Gain curve cannot contain two points at the same frame."};
        }
    }

    return {};
}

double gainDbToLinear(double gainDb) noexcept {
    return std::pow(10.0, gainDb / 20.0);
}

} // namespace pgclip
