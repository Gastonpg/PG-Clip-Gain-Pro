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

    for (std::size_t index = 0U; index < peaks.size(); ++index) {
        const auto& candidate = peaks[index];
        if (!candidate.valid || candidate.silent ||
            activity.windows[index].state != WindowActivityState::Active) {
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
                peaks[neighbor].silent ||
                activity.windows[neighbor].state != WindowActivityState::Active) {
                continue;
            }
            localPeaks.push_back(peaks[neighbor].peakDb);
        }

        if (localPeaks.empty()) {
            continue;
        }

        const double localMedian = median(std::move(localPeaks));
        const double excess = candidate.peakDb - localMedian;

        if (excess <= config.peakExcessThresholdDb ||
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

    std::vector<PeakEvent> consolidated;
    consolidated.reserve(result.events.size());
    for (const auto& event : result.events) {
        if (!consolidated.empty() &&
            event.range.start < consolidated.back().range.end) {
            auto& previous = consolidated.back();
            previous.range.end =
                std::max(previous.range.end, event.range.end);
            previous.confidence =
                std::min(previous.confidence, event.confidence);
            previous.protectedPhraseEdge =
                previous.protectedPhraseEdge || event.protectedPhraseEdge;

            if (previous.action == PeakEventAction::Review ||
                event.action == PeakEventAction::Review ||
                previous.protectedPhraseEdge) {
                previous.action = PeakEventAction::Review;
                previous.suggestedAttenuationDb = 0.0;
            } else {
                previous.suggestedAttenuationDb = std::max(
                    previous.suggestedAttenuationDb,
                    event.suggestedAttenuationDb);
            }

            if (event.excessDb > previous.excessDb) {
                previous.peakDb = event.peakDb;
                previous.localMedianPeakDb = event.localMedianPeakDb;
                previous.excessDb = event.excessDb;
                previous.crestFactor = event.crestFactor;
            }
        } else {
            consolidated.push_back(event);
        }
    }
    result.events = std::move(consolidated);

    result.status = result.windowsScanned > result.invalidWindows
        ? ClipPeakStatus::Ready
        : ClipPeakStatus::NoUsableContext;
    return result;
}

ClipPeakCurveResult buildClipPeakCurve(
    const std::vector<PeakEvent>& events,
    FrameIndex totalFrames,
    const ClipPeakCurveConfig& config) {
    ClipPeakCurveResult result;

    if (totalFrames == 0U ||
        config.rampInFrames == 0U ||
        config.rampOutFrames == 0U) {
        result.status = ClipPeakCurveStatus::InvalidConfiguration;
        result.issue = {ValidationCode::InvalidWindow,
                        "Clip Peak curve requires a non-empty capture and positive ramp lengths."};
        return result;
    }

    std::vector<const PeakEvent*> ordered;
    ordered.reserve(events.size());

    for (const auto& event : events) {
        if (validate(event.range, totalFrames) ||
            !std::isfinite(event.suggestedAttenuationDb) ||
            !std::isfinite(event.confidence) ||
            event.confidence < 0.0 || event.confidence > 1.0 ||
            event.suggestedAttenuationDb > 0.0) {
            result.status = ClipPeakCurveStatus::InvalidEvents;
            result.issue = {ValidationCode::RangeOutsideCapture,
                            "Clip Peak event contains an invalid range, attenuation, or confidence."};
            return result;
        }
        ordered.push_back(&event);
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const auto* left, const auto* right) {
                  return left->range.start < right->range.start;
              });

    for (std::size_t index = 1U; index < ordered.size(); ++index) {
        if (ordered[index]->range.start < ordered[index - 1U]->range.end) {
            result.status = ClipPeakCurveStatus::OverlappingEvents;
            result.issue = {ValidationCode::UnorderedCurvePoints,
                            "Clip Peak events overlap and must be consolidated before curve creation."};
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

    for (const auto* event : ordered) {
        if (event->action != PeakEventAction::Attenuate ||
            event->protectedPhraseEdge) {
            continue;
        }

        if (event->suggestedAttenuationDb >= 0.0) {
            result.status = ClipPeakCurveStatus::InvalidEvents;
            result.issue = {ValidationCode::NonFiniteGain,
                            "An attenuation event must have a negative gain recommendation."};
            result.curve.points.clear();
            return result;
        }

        const FrameIndex duration = event->range.end - event->range.start;
        if (duration < 2U) {
            result.status = ClipPeakCurveStatus::InvalidEvents;
            result.issue = {ValidationCode::EmptyRange,
                            "A peak event needs at least two frames for a smooth ramp."};
            result.curve.points.clear();
            return result;
        }

        const FrameIndex halfDuration = duration / 2U;
        const FrameIndex rampIn =
            std::min(config.rampInFrames, halfDuration);
        const FrameIndex rampOut =
            std::min(config.rampOutFrames, duration - rampIn);
        const FrameIndex peakStart = event->range.start + rampIn;
        const FrameIndex peakEnd = event->range.end - rampOut;

        if (!appendPoint(event->range.start, 0.0) ||
            !appendPoint(peakStart, event->suggestedAttenuationDb) ||
            (peakEnd > peakStart &&
             !appendPoint(peakEnd, event->suggestedAttenuationDb)) ||
            !appendPoint(event->range.end, 0.0)) {
            result.status = ClipPeakCurveStatus::InvalidEvents;
            result.issue = {ValidationCode::DuplicateCurvePoint,
                            "Clip Peak curve points collide with incompatible values."};
            result.curve.points.clear();
            return result;
        }
    }

    if (const auto issue = validate(result.curve)) {
        result.status = ClipPeakCurveStatus::InvalidEvents;
        result.issue = issue;
        result.curve.points.clear();
        return result;
    }

    result.status = ClipPeakCurveStatus::Ready;
    return result;
}

} // namespace pgclip
