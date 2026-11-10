#include "pgclip/core/clip_gain_foundation.hpp"

#include <algorithm>
#include <cmath>
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

    if (activity.status != ActivityDetectionStatus::Ready ||
        activity.regions.empty()) {
        result.status = PhraseProfileStatus::NoCandidateRegions;
        return result;
    }

    const auto channelCount =
        static_cast<std::size_t>(snapshot.format().channelCount);

    result.profiles.reserve(activity.regions.size());

    for (const auto& region : activity.regions) {
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
        profile.levelDb = median(levelValues);
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
            std::isfinite(profile.levelDb)) {
            eligibleLevels.push_back(profile.levelDb);
        }
    }

    result.eligiblePhraseCount =
        static_cast<std::uint32_t>(eligibleLevels.size());
    if (eligibleLevels.size() < config.minimumPhraseCount) {
        result.status = AdaptiveReferenceStatus::NotEnoughPhrases;
        return result;
    }

    std::sort(eligibleLevels.begin(), eligibleLevels.end());
    result.referenceDb = quantileOfSorted(eligibleLevels, config.quantile);

    std::vector<double> deviations;
    deviations.reserve(eligibleLevels.size());
    for (const double level : eligibleLevels) {
        deviations.push_back(std::abs(level - result.referenceDb));
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
        !std::isfinite(reference.referenceDb)) {
        result.status = ClipGainRecommendationStatus::ReferenceUnavailable;
        return result;
    }

    result.recommendations.reserve(profiles.size());

    for (const auto& profile : profiles) {
        PhraseGainRecommendation recommendation;
        recommendation.range = profile.range;
        recommendation.confidence = profile.confidence;

        if (!profile.eligibleForReference ||
            profile.confidence < config.minimumConfidence ||
            !std::isfinite(profile.levelDb)) {
            recommendation.action = GainRecommendationAction::Abstain;
            result.recommendations.push_back(recommendation);
            continue;
        }

        const double relativeDifference =
            reference.referenceDb - profile.levelDb;
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

} // namespace pgclip
