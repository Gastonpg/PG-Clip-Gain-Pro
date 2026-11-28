#include "pgclip/core/five_judges.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pgclip {
namespace {

double clampScore(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

bool validCurve(const GainCurve& curve, FrameIndex totalFrames,
                double maximumCorrectionDb) {
    if (totalFrames == 0U) return false;
    FrameIndex previous = 0U;
    bool first = true;
    for (const auto& point : curve.points) {
        if (point.frame > totalFrames || !std::isfinite(point.gainDb) ||
            std::abs(point.gainDb) > maximumCorrectionDb + 1.0e-9 ||
            (!first && point.frame <= previous)) return false;
        previous = point.frame;
        first = false;
    }
    return true;
}

double evaluateCurveDb(const GainCurve& curve, FrameIndex frame) {
    if (curve.points.empty() || frame < curve.points.front().frame ||
        frame > curve.points.back().frame) return 0.0;

    std::size_t segment = 0U;
    while (segment + 1U < curve.points.size() &&
           frame > curve.points[segment + 1U].frame) ++segment;
    if (segment + 1U >= curve.points.size())
        return curve.points.back().gainDb;

    const auto& left = curve.points[segment];
    const auto& right = curve.points[segment + 1U];
    const double position = static_cast<double>(frame - left.frame) /
        static_cast<double>(right.frame - left.frame);
    const double smooth = position * position * (3.0 - 2.0 * position);
    return left.gainDb + (right.gainDb - left.gainDb) * smooth;
}

bool curveTouchesProtectedRange(
    const GainCurve& curve, const std::vector<PhraseProfile>& phrases) {
    for (const auto& phrase : phrases) {
        for (const auto& range : phrase.protectedRanges) {
            if (range.end <= range.start) return true;
            if (std::abs(evaluateCurveDb(curve, range.start)) > 0.01 ||
                std::abs(evaluateCurveDb(curve, range.end)) > 0.01) return true;
            for (const auto& point : curve.points) {
                if (point.frame >= range.start && point.frame <= range.end &&
                    std::abs(point.gainDb) > 0.01) return true;
            }
            // Check the middle as well: a curve can cross a protected pause
            // even when its explicit control points sit outside the pause.
            const FrameIndex middle = range.start + (range.end - range.start) / 2U;
            if (std::abs(evaluateCurveDb(curve, middle)) > 0.01) return true;
        }
    }
    return false;
}

bool validRange(const FrameRange& range, FrameIndex totalFrames) {
    return range.start < range.end && range.end <= totalFrames;
}

} // namespace

FiveJudgeResult runFiveJudges(
    const std::vector<PhraseProfile>& phrases,
    const AdaptiveReferenceResult& reference,
    const std::vector<PhraseGainRecommendation>& gainRecommendations,
    const ClipPeakResult& peakEvents,
    const GainCurve& proposedCurve,
    FrameIndex totalFrames,
    const FiveJudgeConfig& config) {
    FiveJudgeResult result;

    if (!std::isfinite(config.minimumApprovalScore) ||
        config.minimumApprovalScore < 0.0 ||
        config.minimumApprovalScore > 1.0 ||
        !std::isfinite(config.maximumCorrectionDb) ||
        config.maximumCorrectionDb <= 0.0 ||
        !std::isfinite(config.maximumAdjacentChangeDb) ||
        config.maximumAdjacentChangeDb <= 0.0 ||
        !validCurve(proposedCurve, totalFrames, config.maximumCorrectionDb)) {
        result.globalCoherence.veto = true;
        result.finalArbiter.veto = true;
        return result;
    }

    // Judge 1 — musical interpretation: only trustworthy, measurable
    // phrases contribute. Recommendations must refer to real phrase ranges.
    double phraseQuality = 0.0;
    std::size_t eligibleCount = 0U;
    for (const auto& phrase : phrases) {
        if (!phrase.eligibleForReference) continue;
        if (!validRange(phrase.range, totalFrames) ||
            !std::isfinite(phrase.confidence) ||
            phrase.confidence < 0.0 || phrase.confidence > 1.0 ||
            !std::isfinite(phrase.rmsLevelDb) ||
            !std::isfinite(phrase.peakDb) ||
            !std::isfinite(phrase.crestFactor)) {
            result.musicalInterpretation.veto = true;
            continue;
        }
        // A phrase with a plausible crest factor and valid peak/RMS ordering
        // is more trustworthy than one whose descriptors contradict each other.
        const double crestDb = phrase.peakDb - phrase.rmsLevelDb;
        const double crestRatio = std::pow(10.0, crestDb / 20.0);
        const double crestErrorDb = crestRatio > 0.0 && phrase.crestFactor > 0.0
            ? std::abs(20.0 * std::log10(crestRatio / phrase.crestFactor))
            : 24.0;
        const double crestConsistency = crestErrorDb <= 1.5 ? 1.0 :
            clampScore(1.0 - (crestErrorDb - 1.5) / 12.0);
        phraseQuality += clampScore(phrase.confidence) * crestConsistency;
        ++eligibleCount;
    }
    result.musicalInterpretation.score = eligibleCount == 0U ? 0.0 :
        clampScore(phraseQuality / static_cast<double>(eligibleCount));
    if (eligibleCount == 0U) result.musicalInterpretation.veto = true;

    // Judge 2 — adaptive leveling: the reference must be robust, corrections
    // must move phrases toward it, and each correction must respect its range.
    if (reference.status == AdaptiveReferenceStatus::Ready &&
        std::isfinite(reference.referenceRmsDb) &&
        std::isfinite(reference.medianAbsoluteDeviationDb) &&
        reference.medianAbsoluteDeviationDb >= 0.0 &&
        reference.eligiblePhraseCount >= 2U) {
        result.adaptiveLeveling.score = clampScore(
            1.0 - reference.medianAbsoluteDeviationDb / 12.0);
    } else {
        result.adaptiveLeveling.veto = true;
    }

    std::size_t checkedRecommendations = 0U;
    for (const auto& recommendation : gainRecommendations) {
        if (recommendation.action == GainRecommendationAction::Abstain) continue;
        ++checkedRecommendations;
        if (!validRange(recommendation.range, totalFrames) ||
            !std::isfinite(recommendation.correctionDb) ||
            !std::isfinite(recommendation.confidence) ||
            recommendation.confidence < 0.0 ||
            recommendation.confidence > 1.0 ||
            std::abs(recommendation.correctionDb) >
                config.maximumCorrectionDb + 1.0e-9) {
            result.adaptiveLeveling.veto = true;
            continue;
        }

        const auto phrase = std::find_if(
            phrases.begin(), phrases.end(), [&recommendation](const auto& item) {
                return item.range.start == recommendation.range.start &&
                       item.range.end == recommendation.range.end;
            });
        if (phrase == phrases.end() || !std::isfinite(phrase->rmsLevelDb)) {
            result.adaptiveLeveling.veto = true;
            continue;
        }
        const double needed = reference.referenceRmsDb - phrase->rmsLevelDb;
        if ((needed > 0.10 && recommendation.correctionDb < -0.05) ||
            (needed < -0.10 && recommendation.correctionDb > 0.05)) {
            result.adaptiveLeveling.veto = true;
        }
        if (recommendation.action == GainRecommendationAction::Keep &&
            std::abs(recommendation.correctionDb) > 0.05) {
            result.adaptiveLeveling.veto = true;
        }
    }
    if (checkedRecommendations == 0U) result.adaptiveLeveling.veto = true;
    if (result.adaptiveLeveling.score < 0.20)
        result.adaptiveLeveling.veto = true;

    // Judge 3 — transient protection: malformed or edge-protected peak events
    // cannot be auto-attenuated. Review events must remain at unity.
    double peakQuality = 0.0;
    std::size_t peakCount = 0U;
    if (peakEvents.status == ClipPeakStatus::Ready) {
        for (const auto& event : peakEvents.events) {
            ++peakCount;
            if (!validRange(event.range, totalFrames) ||
                !std::isfinite(event.confidence) ||
                event.confidence < 0.0 || event.confidence > 1.0 ||
                !std::isfinite(event.peakDb) ||
                !std::isfinite(event.localMedianPeakDb) ||
                !std::isfinite(event.excessDb) ||
                !std::isfinite(event.crestFactor) ||
                !std::isfinite(event.suggestedAttenuationDb) ||
                event.suggestedAttenuationDb > 0.0) {
                result.transientProtection.veto = true;
                continue;
            }
            if (event.protectedPhraseEdge &&
                (event.action == PeakEventAction::Attenuate ||
                 event.suggestedAttenuationDb < -0.001)) {
                result.transientProtection.veto = true;
            }
            if (event.action == PeakEventAction::Review &&
                std::abs(event.suggestedAttenuationDb) > 0.001) {
                result.transientProtection.veto = true;
            }
            peakQuality += clampScore(event.confidence);
        }
        // No detected peak anomalies is a valid and safe outcome.
        result.transientProtection.score = peakCount == 0U ? 1.0 :
            clampScore(peakQuality / static_cast<double>(peakCount));
    } else {
        result.transientProtection.veto = true;
    }

    // Judge 4 — global coherence: avoid abrupt clip-gain jumps and verify the
    // entire protected interval, not only explicit curve control points.
    result.globalCoherence.score = 1.0;
    for (std::size_t i = 1U; i < proposedCurve.points.size(); ++i) {
        const double change = std::abs(
            proposedCurve.points[i].gainDb -
            proposedCurve.points[i - 1U].gainDb);
        if (change > config.maximumAdjacentChangeDb) {
            result.globalCoherence.score = std::min(
                result.globalCoherence.score,
                clampScore(config.maximumAdjacentChangeDb / change));
        }
    }
    if (curveTouchesProtectedRange(proposedCurve, phrases)) {
        result.globalCoherence.veto = true;
        result.globalCoherence.score = 0.0;
    }

    // Judge 5 — final arbitration. Any safety veto blocks the curve; the
    // confidence factor only tempers an approved static edit, never audio DSP.
    const double consensus =
        0.30 * result.musicalInterpretation.score +
        0.25 * result.adaptiveLeveling.score +
        0.25 * result.transientProtection.score +
        0.20 * result.globalCoherence.score;
    const bool specialistVeto =
        result.musicalInterpretation.veto ||
        result.adaptiveLeveling.veto ||
        result.transientProtection.veto ||
        result.globalCoherence.veto;
    result.finalArbiter.score = clampScore(consensus);
    result.confidence = result.finalArbiter.score;
    result.approved = !specialistVeto &&
        result.finalArbiter.score >= config.minimumApprovalScore;
    result.finalArbiter.veto = !result.approved;
    result.evaluated = true;

    if (result.approved) {
        result.approvedCurve.points.reserve(proposedCurve.points.size());
        for (const auto& point : proposedCurve.points) {
            result.approvedCurve.points.push_back(
                {point.frame, point.gainDb * result.confidence});
        }
    }
    return result;
}

} // namespace pgclip
