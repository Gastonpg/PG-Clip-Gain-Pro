#include "pgclip/core/clip_peak_foundation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace pgclip {
namespace {

ValidationIssue validate(const ClipPeakConfig& config) {
    if (config.localRadiusWindows == 0U ||
        !std::isfinite(config.peakExcessThresholdDb) ||
        config.peakExcessThresholdDb <= 0.0 ||
        !std::isfinite(config.minimumCrestFactor) ||
        config.minimumCrestFactor <= 1.0 ||
        !std::isfinite(config.maximumAttenuationDb) ||
        config.maximumAttenuationDb <= 0.0 ||
        !std::isfinite(config.minimumConfidence) ||
        config.minimumConfidence < 0.0 ||
        config.minimumConfidence > 1.0) {
        return {ValidationCode::InvalidWindow,
                "Clip Peak thresholds, local radius, attenuation or confidence are invalid."};
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

bool isProtectedPhraseEdge(
    const WindowActivity& window,
    const std::vector<ActivityRegionCandidate>& regions,
    FrameIndex edgeSpan) {
    for (const auto& region : regions) {
        const bool nearStart =
            window.range.start >= region.range.start &&
            window.range.start - region.range.start < edgeSpan;
        const bool nearEnd =
            window.range.end <= region.range.end &&
            region.range.end - window.range.end < edgeSpan;
        if (nearStart || nearEnd) {
            return true;
        }
    }
    return false;
}

} // namespace

ClipPeakResult detectClipPeakEvents(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionResult& activity,
    const ClipPeakConfig& config) {
    ClipPeakResult result;

    if (const auto issue = validate(config)) {
        result.status = ClipPeakStatus::InvalidConfiguration;
        result.issue = issue;
        return result;
    }

    if (activity.status != ActivityDetectionStatus::Ready ||
        activity.windows.size() != snapshot.windows().size()) {
        result.status = ClipPeakStatus::InvalidContext;
        result.issue = {ValidationCode::InvalidWindow,
                        "Clip Peak requires an activity map from the same analysis window grid."};
        return result;
    }

    for (std::size_t index = 0U; index < activity.windows.size(); ++index) {
        if (activity.windows[index].range.start !=
                snapshot.windows()[index].range.start ||
            activity.windows[index].range.end !=
                snapshot.windows()[index].range.end) {
            result.status = ClipPeakStatus::InvalidContext;
            result.issue = {ValidationCode::RangeOutsideCapture,
                            "Clip Peak activity context does not match the snapshot."};
            return result;
        }
    }

    const auto channelCount =
        static_cast<std::size_t>(snapshot.format().channelCount);
    const FrameIndex windowFrames = snapshot.config().windowFrames;
    const FrameIndex edgeWindowCount =
        static_cast<FrameIndex>(config.protectPhraseEdgeWindows);
    const FrameIndex edgeSpan =
        edgeWindowCount > 0U &&
        windowFrames > std::numeric_limits<FrameIndex>::max() / edgeWindowCount
            ? snapshot.totalFrames()
            : windowFrames * edgeWindowCount;

    struct WindowPeak {
        bool valid{false};
        bool silent{true};
        double peakDb{0.0};
        double crestFactor{0.0};
    };

    std::vector<WindowPeak> peaks(snapshot.windows().size());

    for (std::size_t index = 0U; index < snapshot.windows().size(); ++index) {
        ++result.windowsScanned;
        const auto& window = snapshot.windows()[index];
        bool allInvalid = true;
        bool hasInvalidChannel = false;
        double peak = 0.0;
        double crest = 0.0;

        for (std::size_t channel = 0U; channel < channelCount; ++channel) {
            const auto& metrics = window.channels[channel];
            if (!metrics.finite) {
                hasInvalidChannel = true;
                continue;
            }

            allInvalid = false;
            if (!metrics.silent) {
                peak = std::max(peak, metrics.peak);
                crest = std::max(crest, metrics.crestFactor);
            }
        }

        if (allInvalid || hasInvalidChannel) {
            ++result.invalidWindows;
            continue;
        }

        if (peak <= 0.0) {
            peaks[index].valid = true;
            peaks[index].silent = true;
            continue;
        }

        peaks[index].valid = true;
        peaks[index].silent = false;
        peaks[index].peakDb = 20.0 * std::log10(peak);
        peaks[index].crestFactor = crest;
    }

    const FrameIndex totalFrames = snapshot.totalFrames();

    for (std::size_t index = 0U; index < peaks.size(); ++index) {
        const auto& candidate = peaks[index];
        if (!candidate.valid || candidate.silent) {
            continue;
        }

        const auto contextRadius = static_cast<std::size_t>(
            config.localRadiusWindows);
        const auto first = index > contextRadius ? index - contextRadius : 0U;
        const auto last = std::min(
            peaks.size() - 1U, index + contextRadius);

        std::vector<double> localPeaks;
        localPeaks.reserve(last - first);
        for (std::size_t neighbor = first; neighbor <= last; ++neighbor) {
            if (neighbor == index || !peaks[neighbor].valid ||
                peaks[neighbor].silent) {
                continue;
            }
            localPeaks.push_back(peaks[neighbor].peakDb);
        }

        if (localPeaks.empty()) {
            continue;
        }

        const double localMedian = median(std::move(localPeaks));
        const double excess = candidate.peakDb - localMedian;

        if (excess < config.peakExcessThresholdDb ||
            candidate.crestFactor < config.minimumCrestFactor) {
            continue;
        }

        PeakEvent event;
        event.range = snapshot.windows()[index].range;
        event.peakDb = candidate.peakDb;
        event.localMedianPeakDb = localMedian;
        event.excessDb = excess;
        event.crestFactor = candidate.crestFactor;
        event.confidence = std::clamp(
            (excess - config.peakExcessThresholdDb) /
                std::max(config.peakExcessThresholdDb, 1.0),
            0.0, 1.0);
        event.protectedPhraseEdge = isProtectedPhraseEdge(
            activity.windows[index], activity.regions, edgeSpan);

        if (event.protectedPhraseEdge ||
            event.confidence < config.minimumConfidence) {
            event.action = PeakEventAction::Review;
            event.suggestedAttenuationDb = 0.0;
        } else {
            event.action = PeakEventAction::Attenuate;
            event.suggestedAttenuationDb = -std::min(
                config.maximumAttenuationDb,
                excess - config.peakExcessThresholdDb);
        }

        result.events.push_back(event);
    }

    result.status = result.windowsScanned > result.invalidWindows
        ? ClipPeakStatus::Ready
        : ClipPeakStatus::NoUsableContext;
    return result;
}

} // namespace pgclip
