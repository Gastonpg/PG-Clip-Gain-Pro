#include "pgclip/core/clip_gain_foundation.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace pgclip {
namespace {

ValidationIssue validate(const PhraseProfileConfig& config) {
    if (config.minimumActiveWindows == 0U ||
        !std::isfinite(config.minimumConfidence) ||
        config.minimumConfidence < 0.0 ||
        config.minimumConfidence > 1.0) {
        return {ValidationCode::InvalidWindow,
                "Phrase profile minimum windows or confidence is invalid."};
    }
    return {};
}

ValidationIssue validate(const AdaptiveReferenceConfig& config) {
    if (config.minimumPhraseCount == 0U ||
        !std::isfinite(config.quantile) ||
        config.quantile < 0.0 || config.quantile > 1.0) {
        return {ValidationCode::InvalidWindow,
                "Adaptive reference phrase count or quantile is invalid."};
    }
    return {};
}

ValidationIssue validate(const ClipGainRecommendationConfig& config) {
    if (!std::isfinite(config.correctionStrength) ||
        config.correctionStrength < 0.0 ||
        config.correctionStrength > 1.0 ||
        !std::isfinite(config.maximumBoostDb) ||
        config.maximumBoostDb < 0.0 ||
        !std::isfinite(config.maximumCutDb) ||
        config.maximumCutDb < 0.0 ||
        !std::isfinite(config.minimumConfidence) ||
        config.minimumConfidence < 0.0 ||
        config.minimumConfidence > 1.0) {
        return {ValidationCode::InvalidGain,
                "Clip Gain correction strength, limits, or confidence are invalid."};
    }
    return {};
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2U;
    if ((values.size() % 2U) != 0U) {
        return values[middle];
    }
    return values[middle - 1U] +
           (values[middle] - values[middle - 1U]) * 0.5;
}

double quantileOfSorted(const std::vector<double>& sorted, double quantile) {
    const double position =
        quantile * static_cast<double>(sorted.size() - 1U);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) {
        return sorted[lower];
    }
    const double fraction = position - static_cast<double>(lower);
    return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
}

double toDb(double amplitude) {
    return 20.0 * std::log10(amplitude);
}

} // namespace

PhraseProfileResult analyzePhraseProfiles(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionResult& activity,
    const PhraseProfileConfig& config) {
    PhraseProfileResult result;

    if (const auto issue = validate(config)) {
        result.status = PhraseProfileStatus::InvalidConfiguration;
        result.issue = issue;
        return result;
    }

    if (activity.status == ActivityDetectionStatus::InvalidConfiguration) {
        result.status = PhraseProfileStatus::InvalidConfiguration;
        result.issue = activity.issue;
        return result;
    }

    if (activity.status != ActivityDetectionStatus::Ready ||
        activity.regions.empty() ||
        activity.windows.size() != snapshot.windows().size()) {
        result.status = PhraseProfileStatus::NoCandidateRegions;
        return result;
    }

    for (std::size_t index = 0U; index < activity.windows.size(); ++index) {
        const auto& activityRange = activity.windows[index].range;
        const auto& snapshotRange = snapshot.windows()[index].range;
        if (activityRange.start != snapshotRange.start ||
            activityRange.end != snapshotRange.end) {
            result.status = PhraseProfileStatus::InvalidConfiguration;
            result.issue = {ValidationCode::RangeOutsideCapture,
                            "Activity map does not belong to this snapshot window grid."};
            return result;
        }
    }

    const auto channelCount =
        static_cast<std::size_t>(snapshot.format().channelCount);

    result.profiles.reserve(activity.regions.size());

    for (const auto& region : activity.regions) {
        if (validate(region.range, snapshot.totalFrames()) ||
            !std::isfinite(region.confidence) ||
            region.confidence < 0.0 || region.confidence > 1.0) {
            result.status = PhraseProfileStatus::InvalidConfiguration;
            result.issue = {ValidationCode::RangeOutsideCapture,
                            "Activity region range or confidence does not match the snapshot."};
            result.profiles.clear();
            return result;
        }

        std::vector<double> levelValues;
        std::vector<double> peakValues;
        std::vector<double> meanAbsoluteValues;
        std::vector<double> crestValues;

        for (std::size_t windowIndex = 0U;
             windowIndex < activity.windows.size() &&
             windowIndex < snapshot.windows().size();
             ++windowIndex) {
            const auto& activityWindow = activity.windows[windowIndex];
            if (activityWindow.state != WindowActivityState::Active ||
                activityWindow.range.start < region.range.start ||
                activityWindow.range.end > region.range.end) {
                continue;
            }

            const auto& sourceWindow = snapshot.windows()[windowIndex];
            bool valid = true;
            double rms = 0.0;
            double peak = 0.0;
            double meanAbsolute = 0.0;
            double crest = 0.0;

            for (std::size_t channel = 0U;
                 channel < channelCount; ++channel) {
                const auto& metrics = sourceWindow.channels[channel];
                if (!metrics.finite) {
                    valid = false;
                    break;
                }
                rms = std::max(rms, metrics.rms);
                peak = std::max(peak, metrics.peak);
                meanAbsolute = std::max(meanAbsolute, metrics.meanAbsolute);
                crest = std::max(crest, metrics.crestFactor);
            }

            if (!valid || rms <= 0.0 || peak <= 0.0 ||
                meanAbsolute <= 0.0) {
                continue;
            }

            levelValues.push_back(toDb(rms));
            peakValues.push_back(toDb(peak));
            meanAbsoluteValues.push_back(toDb(meanAbsolute));
            crestValues.push_back(crest);
        }

        if (levelValues.empty()) {
            continue;
        }

        PhraseProfile profile;
        profile.range = region.range;
        profile.rmsLevelDb = median(levelValues);
        profile.peakDb = median(peakValues);
        profile.meanAbsoluteDb = median(meanAbsoluteValues);
        profile.crestFactor = median(crestValues);
        profile.activeWindowCount =
            static_cast<std::uint32_t>(levelValues.size());
        profile.confidence = region.confidence;
        profile.eligibleForReference =
            profile.activeWindowCount >= config.minimumActiveWindows &&
            profile.confidence >= config.minimumConfidence;

        result.profiles.push_back(profile);
    }

    result.status = result.profiles.empty()
        ? PhraseProfileStatus::NoCandidateRegions
        : PhraseProfileStatus::Ready;
    return result;
}

AdaptiveReferenceResult selectAdaptiveReference(
    const std::vector<PhraseProfile>& profiles,
    const AdaptiveReferenceConfig& config) {
    AdaptiveReferenceResult result;

    if (const auto issue = validate(config)) {
        result.status = AdaptiveReferenceStatus::InvalidConfiguration;
        result.issue = issue;
        return result;
    }

    std::vector<double> eligibleLevels;
    eligibleLevels.reserve(profiles.size());
    for (const auto& profile : profiles) {
        if (profile.eligibleForReference &&
            std::isfinite(profile.rmsLevelDb) &&
            std::isfinite(profile.confidence) &&
            profile.confidence >= 0.0 && profile.confidence <= 1.0) {
            eligibleLevels.push_back(profile.rmsLevelDb);
        }
    }

    result.eligiblePhraseCount =
        static_cast<std::uint32_t>(eligibleLevels.size());
    if (eligibleLevels.size() < config.minimumPhraseCount) {
        result.status = AdaptiveReferenceStatus::NotEnoughPhrases;
        return result;
    }

    std::sort(eligibleLevels.begin(), eligibleLevels.end());
    result.referenceRmsDb = quantileOfSorted(eligibleLevels, config.quantile);

    std::vector<double> deviations;
    deviations.reserve(eligibleLevels.size());
    for (const double level : eligibleLevels) {
        deviations.push_back(std::abs(level - result.referenceRmsDb));
    }
    result.medianAbsoluteDeviationDb = median(std::move(deviations));
    result.status = AdaptiveReferenceStatus::Ready;
    return result;
}

ClipGainRecommendationResult recommendClipGain(
    const std::vector<PhraseProfile>& profiles,
    const AdaptiveReferenceResult& reference,
    const ClipGainRecommendationConfig& config) {
    ClipGainRecommendationResult result;

    if (const auto issue = validate(config)) {
        result.status = ClipGainRecommendationStatus::InvalidConfiguration;
        result.issue = issue;
        return result;
    }

    if (reference.status != AdaptiveReferenceStatus::Ready ||
        !std::isfinite(reference.referenceRmsDb)) {
        result.status = ClipGainRecommendationStatus::ReferenceUnavailable;
        return result;
    }

    result.recommendations.reserve(profiles.size());

    for (const auto& profile : profiles) {
        PhraseGainRecommendation recommendation;
        recommendation.range = profile.range;
        recommendation.confidence = profile.confidence;

        if (!profile.eligibleForReference ||
            !std::isfinite(profile.confidence) ||
            profile.confidence < config.minimumConfidence ||
            profile.confidence > 1.0 ||
            profile.range.end <= profile.range.start ||
            !std::isfinite(profile.rmsLevelDb)) {
            recommendation.action = GainRecommendationAction::Abstain;
            result.recommendations.push_back(recommendation);
            continue;
        }

        const double relativeDifference =
            reference.referenceRmsDb - profile.rmsLevelDb;
        recommendation.correctionDb = std::clamp(
            relativeDifference * config.correctionStrength,
            -config.maximumCutDb,
            config.maximumBoostDb);

        recommendation.action =
            std::abs(recommendation.correctionDb) < 0.05
                ? GainRecommendationAction::Keep
                : GainRecommendationAction::Adjust;

        result.recommendations.push_back(recommendation);
    }

    result.status = ClipGainRecommendationStatus::Ready;
    return result;
}


ClipGainCurveResult buildClipGainCurve(
    const std::vector<PhraseGainRecommendation>& recommendations,
    FrameIndex totalFrames,
    const ClipGainCurveConfig& config) {
    ClipGainCurveResult result;

    if (totalFrames == 0U ||
        config.rampInFrames == 0U ||
        config.rampOutFrames == 0U) {
        result.status = ClipGainCurveStatus::InvalidConfiguration;
        result.issue = {ValidationCode::InvalidWindow,
                        "Curve generation requires a non-empty capture and positive ramp lengths."};
        return result;
    }

    std::vector<const PhraseGainRecommendation*> adjustments;
    adjustments.reserve(recommendations.size());

    for (const auto& recommendation : recommendations) {
        if (recommendation.action != GainRecommendationAction::Adjust) {
            continue;
        }

        if (validate(recommendation.range, totalFrames) ||
            !std::isfinite(recommendation.correctionDb) ||
            !std::isfinite(recommendation.confidence) ||
            recommendation.confidence < 0.0 ||
            recommendation.confidence > 1.0) {
            result.status = ClipGainCurveStatus::InvalidRecommendations;
            result.issue = {ValidationCode::RangeOutsideCapture,
                            "A gain recommendation has an invalid range, gain, or confidence."};
            return result;
        }

        adjustments.push_back(&recommendation);
    }

    std::sort(adjustments.begin(), adjustments.end(),
              [](const auto* left, const auto* right) {
                  return left->range.start < right->range.start;
              });

    for (std::size_t index = 1U; index < adjustments.size(); ++index) {
        if (adjustments[index]->range.start <
            adjustments[index - 1U]->range.end) {
            result.status = ClipGainCurveStatus::OverlappingRegions;
            result.issue = {ValidationCode::UnorderedCurvePoints,
                            "Overlapping phrase recommendations cannot be merged safely."};
            return result;
        }
    }

    const auto appendPoint = [&result](FrameIndex frame, double gainDb) {
        if (!result.curve.points.empty() &&
            result.curve.points.back().frame == frame) {
            return std::abs(result.curve.points.back().gainDb - gainDb) < 1.0e-9;
        }
        result.curve.points.push_back({frame, gainDb});
        return true;
    };

    for (const auto* recommendation : adjustments) {
        const FrameIndex duration =
            recommendation->range.end - recommendation->range.start;
        if (duration < 2U) {
            result.status = ClipGainCurveStatus::InvalidRecommendations;
            result.issue = {ValidationCode::EmptyRange,
                            "A phrase needs at least two frames for a non-zero ramp."};
            result.curve.points.clear();
            return result;
        }

        const FrameIndex halfDuration = duration / 2U;
        const FrameIndex rampIn =
            std::min(config.rampInFrames, halfDuration);
        const FrameIndex rampOut =
            std::min(config.rampOutFrames, duration - rampIn);

        const FrameIndex peakStart = recommendation->range.start + rampIn;
        const FrameIndex peakEnd = recommendation->range.end - rampOut;

        if (!appendPoint(recommendation->range.start, 0.0) ||
            !appendPoint(peakStart, recommendation->correctionDb) ||
            (peakEnd > peakStart &&
             !appendPoint(peakEnd, recommendation->correctionDb)) ||
            !appendPoint(recommendation->range.end, 0.0)) {
            result.status = ClipGainCurveStatus::InvalidRecommendations;
            result.issue = {ValidationCode::DuplicateCurvePoint,
                            "Curve points collide with incompatible gain values."};
            result.curve.points.clear();
            return result;
        }
    }

    if (const auto issue = validate(result.curve)) {
        result.status = ClipGainCurveStatus::InvalidRecommendations;
        result.issue = issue;
        result.curve.points.clear();
        return result;
    }

    result.status = ClipGainCurveStatus::Ready;
    return result;
}

} // namespace pgclip
