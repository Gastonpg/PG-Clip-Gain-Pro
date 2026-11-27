#include "pgclip/core/five_judges.hpp"

#include <algorithm>
#include <cmath>

namespace pgclip {
namespace {

double clampScore(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

bool validCurve(const GainCurve& curve, FrameIndex totalFrames,
                double maximumCorrectionDb) {
    if (totalFrames == 0U || curve.points.empty()) return false;
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

bool pointTouchesProtectedRange(const GainCurve& curve,
                                const std::vector<PhraseProfile>& phrases) {
    for (const auto& phrase : phrases) {
        for (const auto& protectedRange : phrase.protectedRanges) {
            for (const auto& point : curve.points) {
                if (point.frame >= protectedRange.start &&
                    point.frame <= protectedRange.end &&
                    std::abs(point.gainDb) > 0.01) return true;
            }
        }
    }
    return false;
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

    // Judge 1: musical/phrase interpretation. Low-confidence phrases abstain;
    // bridged pauses remain protected and are checked again by the arbiter.
    double phraseConfidence = 0.0;
    std::size_t phraseCount = 0U;
    for (const auto& phrase : phrases) {
        if (phrase.eligibleForReference && std::isfinite(phrase.confidence)) {
            phraseConfidence += clampScore(phrase.confidence);
            ++phraseCount;
        }
    }
    result.musicalInterpretation.score = phraseCount == 0U
        ? 0.0 : clampScore(phraseConfidence / static_cast<double>(phraseCount));
    result.musicalInterpretation.veto = phraseCount == 0U;

    // Judge 2: adaptive leveling. It rewards a stable relative reference and
    // rejects corrections that exceed the declared manual Clip Gain limits.
    if (reference.status == AdaptiveReferenceStatus::Ready &&
        std::isfinite(reference.referenceRmsDb) &&
        std::isfinite(reference.medianAbsoluteDeviationDb)) {
        result.adaptiveLeveling.score = clampScore(
            1.0 - reference.medianAbsoluteDeviationDb / 18.0);
    }
    for (const auto& recommendation : gainRecommendations) {
        if (!std::isfinite(recommendation.correctionDb) ||
            std::abs(recommendation.correctionDb) >
                config.maximumCorrectionDb + 1.0e-9) {
            result.adaptiveLeveling.veto = true;
        }
    }
    if (result.adaptiveLeveling.score < 0.25)
        result.adaptiveLeveling.veto = true;

    // Judge 3: transient/peak protection. Phrase-edge events must never be
    // attenuated automatically; review-only events remain untouched.
    double peakConfidence = 0.0;
    std::size_t peakCount = 0U;
    if (peakEvents.status == ClipPeakStatus::Ready) {
        for (const auto& event : peakEvents.events) {
            ++peakCount;
            peakConfidence += clampScore(event.confidence);
            if (event.protectedPhraseEdge &&
                (event.action == PeakEventAction::Attenuate ||
                 event.suggestedAttenuationDb < -0.001)) {
                result.transientProtection.veto = true;
            }
            if (event.suggestedAttenuationDb > 0.0 ||
                !std::isfinite(event.suggestedAttenuationDb)) {
                result.transientProtection.veto = true;
            }
        }
        result.transientProtection.score = peakCount == 0U
            ? 1.0 : clampScore(peakConfidence / static_cast<double>(peakCount));
    } else {
        result.transientProtection.veto = true;
    }

    // Judge 4: global coherence. It rejects abrupt discontinuities and any
    // non-zero curve point placed inside a protected pause.
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
    if (pointTouchesProtectedRange(proposedCurve, phrases)) {
        result.globalCoherence.veto = true;
        result.globalCoherence.score = 0.0;
    }

    // Judge 5: independent arbitration. Any specialist veto is a hard stop;
    // otherwise the weighted consensus scales the approved static correction.
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
    result.finalArbiter.veto = specialistVeto;
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
