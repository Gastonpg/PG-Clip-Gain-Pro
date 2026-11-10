#include "pgclip/core/window_activity.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pgclip {
namespace {

ValidationIssue validate(const ActivityDetectionConfig& config) {
    if (!std::isfinite(config.enterMarginDb) ||
        !std::isfinite(config.exitMarginDb) ||
        config.enterMarginDb <= 0.0 ||
        config.exitMarginDb < 0.0 ||
        config.exitMarginDb >= config.enterMarginDb) {
        return {ValidationCode::InvalidWindow,
                "Activity margins must be finite, positive, and use a lower exit threshold."};
    }

    if (!std::isfinite(config.lowerQuantile) ||
        config.lowerQuantile <= 0.0 ||
        config.lowerQuantile > 0.5) {
        return {ValidationCode::InvalidWindow,
                "Noise-floor quantile must be in the interval (0, 0.5]."};
    }

    if (!std::isfinite(config.minimumContrastDb) ||
        config.minimumContrastDb <= 0.0 ||
        config.minimumActiveWindows == 0U) {
        return {ValidationCode::InvalidWindow,
                "Activity contrast and minimum active-run configuration are invalid."};
    }

    return {};
}

double quantileOfSorted(const std::vector<double>& sorted, double quantile) {
    const auto last = sorted.size() - 1U;
    const double position = quantile * static_cast<double>(last);
    const auto lowerIndex = static_cast<std::size_t>(std::floor(position));
    const auto upperIndex = static_cast<std::size_t>(std::ceil(position));

    if (lowerIndex == upperIndex) {
        return sorted[lowerIndex];
    }

    const double fraction = position - static_cast<double>(lowerIndex);
    return sorted[lowerIndex] +
           (sorted[upperIndex] - sorted[lowerIndex]) * fraction;
}

double confidenceFromMargin(double marginDb, double scaleDb) {
    return std::clamp(marginDb / std::max(scaleDb, 1.0), 0.0, 1.0);
}

} // namespace

ActivityDetectionResult detectWindowActivity(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionConfig& config) {
    ActivityDetectionResult result;

    if (const auto issue = validate(config)) {
        result.status = ActivityDetectionStatus::InvalidConfiguration;
        result.issue = issue;
        return result;
    }

    const auto channelCount =
        static_cast<std::size_t>(snapshot.format().channelCount);
    if (channelCount == 0U || snapshot.windows().empty()) {
        result.status = ActivityDetectionStatus::NoUsableWindows;
        return result;
    }

    result.windows.resize(snapshot.windows().size());
    std::vector<double> nonSilentLevels;
    nonSilentLevels.reserve(snapshot.windows().size());

    for (std::size_t index = 0U;
         index < snapshot.windows().size(); ++index) {
        const auto& sourceWindow = snapshot.windows()[index];
        auto& destination = result.windows[index];
        destination.range = sourceWindow.range;

        bool hasFiniteChannel = false;
        bool hasInvalidChannel = false;
        bool hasNonSilentChannel = false;
        double representativeRms = 0.0;

        for (std::size_t channel = 0U; channel < channelCount; ++channel) {
            const auto& metrics = sourceWindow.channels[channel];
            if (!metrics.finite) {
                hasInvalidChannel = true;
                continue;
            }

            hasFiniteChannel = true;
            if (!metrics.silent) {
                hasNonSilentChannel = true;
                representativeRms = std::max(representativeRms, metrics.rms);
            }
        }

        if (!hasFiniteChannel) {
            destination.state = WindowActivityState::Invalid;
            ++result.invalidWindows;
            continue;
        }

        ++result.validWindows;
        if (hasInvalidChannel) {
            destination.state = WindowActivityState::Uncertain;
            destination.confidence = 0.1;
            continue;
        }

        if (!hasNonSilentChannel || representativeRms <= 0.0) {
            destination.levelDb = -std::numeric_limits<double>::infinity();
            destination.state = WindowActivityState::DigitalSilence;
            ++result.silentWindows;
            continue;
        }

        destination.levelDb = 20.0 * std::log10(representativeRms);
        destination.state = WindowActivityState::BelowThreshold;
        nonSilentLevels.push_back(destination.levelDb);
    }

    if (nonSilentLevels.empty()) {
        result.status = ActivityDetectionStatus::NoUsableWindows;
        return result;
    }

    std::sort(nonSilentLevels.begin(), nonSilentLevels.end());
    result.noiseFloorDb =
        quantileOfSorted(nonSilentLevels, config.lowerQuantile);

    const double contrast =
        quantileOfSorted(nonSilentLevels, 0.8) -
        quantileOfSorted(nonSilentLevels, 0.2);
    if (contrast < std::max(config.minimumContrastDb,
                            config.enterMarginDb)) {
        result.status = ActivityDetectionStatus::InsufficientContrast;
        for (auto& window : result.windows) {
            if (window.state == WindowActivityState::BelowThreshold) {
                window.state = WindowActivityState::Uncertain;
                window.confidence = 0.2;
            }
        }
        return result;
    }

    result.enterThresholdDb =
        result.noiseFloorDb + config.enterMarginDb;
    result.exitThresholdDb =
        result.noiseFloorDb + config.exitMarginDb;

    bool active = false;
    for (auto& window : result.windows) {
        if (window.state == WindowActivityState::Invalid ||
            window.state == WindowActivityState::DigitalSilence) {
            active = false;
            continue;
        }

        if (active) {
            if (window.levelDb >= result.exitThresholdDb) {
                window.state = WindowActivityState::Active;
                window.confidence = confidenceFromMargin(
                    window.levelDb - result.exitThresholdDb,
                    config.enterMarginDb);
            } else {
                active = false;
                window.state = WindowActivityState::BelowThreshold;
            }
        } else if (window.levelDb >= result.enterThresholdDb) {
            active = true;
            window.state = WindowActivityState::Active;
            window.confidence = confidenceFromMargin(
                window.levelDb - result.enterThresholdDb,
                config.enterMarginDb);
        }
    }

    // Bridge only short, non-invalid gaps between two active regions.
    if (config.bridgeGapWindows > 0U) {
        std::size_t index = 0U;
        while (index < result.windows.size()) {
            if (result.windows[index].state != WindowActivityState::Active) {
                ++index;
                continue;
            }

            std::size_t gapStart = index + 1U;
            while (gapStart < result.windows.size() &&
                   result.windows[gapStart].state != WindowActivityState::Active &&
                   result.windows[gapStart].state != WindowActivityState::Invalid) {
                ++gapStart;
            }

            const auto gapLength = gapStart - (index + 1U);
            if (gapStart < result.windows.size() &&
                result.windows[gapStart].state == WindowActivityState::Active &&
                gapLength > 0U &&
                gapLength <= config.bridgeGapWindows) {
                for (std::size_t gap = index + 1U; gap < gapStart; ++gap) {
                    result.windows[gap].state = WindowActivityState::BridgedPause;
                    result.windows[gap].confidence = 0.25;
                }
            }

            index = gapStart;
        }
    }

    // Very short active runs remain visible, but are marked uncertain.
    std::size_t index = 0U;
    while (index < result.windows.size()) {
        if (result.windows[index].state != WindowActivityState::Active) {
            ++index;
            continue;
        }

        const auto runStart = index;
        std::uint32_t activeCount = 0U;
        while (index < result.windows.size() &&
               (result.windows[index].state == WindowActivityState::Active ||
                result.windows[index].state == WindowActivityState::BridgedPause)) {
            if (result.windows[index].state == WindowActivityState::Active) {
                ++activeCount;
            }
            ++index;
        }

        if (activeCount < config.minimumActiveWindows) {
            for (std::size_t run = runStart; run < index; ++run) {
                if (result.windows[run].state == WindowActivityState::Active ||
                    result.windows[run].state == WindowActivityState::BridgedPause) {
                    result.windows[run].state = WindowActivityState::Uncertain;
                    result.windows[run].confidence =
                        std::min(result.windows[run].confidence, 0.25);
                }
            }
        }
    }

    result.status = ActivityDetectionStatus::Ready;
    return result;
}

} // namespace pgclip
