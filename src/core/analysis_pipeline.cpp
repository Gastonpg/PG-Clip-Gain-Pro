#include "pgclip/core/analysis_pipeline.hpp"

namespace pgclip {

AnalysisPipelineResult buildAnalysisPipeline(
    const SharedAnalysisSnapshot& snapshot,
    const AnalysisPipelineConfig& config) {
    AnalysisPipelineResult result;

    result.activity = detectWindowActivity(snapshot, config.activity);
    if (result.activity.status == ActivityDetectionStatus::InvalidConfiguration) {
        result.status = AnalysisPipelineStatus::InvalidActivity;
        result.issue = result.activity.issue;
        return result;
    }
    if (result.activity.status != ActivityDetectionStatus::Ready) {
        result.status = AnalysisPipelineStatus::NoReliableActivity;
        result.issue = result.activity.issue;
        return result;
    }

    result.phraseProfiles = analyzePhraseProfiles(
        snapshot, result.activity, config.phraseProfiles);
    if (result.phraseProfiles.status != PhraseProfileStatus::Ready) {
        result.status = AnalysisPipelineStatus::InvalidPhraseProfiles;
        result.issue = result.phraseProfiles.issue;
        return result;
    }

    result.reference = selectAdaptiveReference(
        result.phraseProfiles.profiles, config.adaptiveReference);
    if (result.reference.status != AdaptiveReferenceStatus::Ready) {
        result.status = AnalysisPipelineStatus::ReferenceUnavailable;
        result.issue = result.reference.issue;
        return result;
    }

    result.clipGainRecommendations = recommendClipGain(
        result.phraseProfiles.profiles, result.reference, config.clipGain);
    if (result.clipGainRecommendations.status !=
        ClipGainRecommendationStatus::Ready) {
        result.status = AnalysisPipelineStatus::InvalidClipGain;
        result.issue = result.clipGainRecommendations.issue;
        return result;
    }

    const auto gainCurve = buildClipGainCurve(
        result.clipGainRecommendations.recommendations,
        snapshot.totalFrames(), config.clipGainCurve);
    if (gainCurve.status != ClipGainCurveStatus::Ready) {
        result.status = AnalysisPipelineStatus::InvalidClipGainCurve;
        result.issue = gainCurve.issue;
        return result;
    }
    result.clipGainCurve = gainCurve.curve;

    result.clipPeakEvents = detectClipPeakEvents(
        snapshot, result.activity, config.clipPeak);
    if (result.clipPeakEvents.status != ClipPeakStatus::Ready) {
        result.status = AnalysisPipelineStatus::InvalidClipPeak;
        result.issue = result.clipPeakEvents.issue;
        return result;
    }

    const auto peakCurve = buildClipPeakCurve(
        result.clipPeakEvents.events,
        snapshot.totalFrames(), config.clipPeakCurve);
    if (peakCurve.status != ClipPeakCurveStatus::Ready) {
        result.status = AnalysisPipelineStatus::InvalidClipPeakCurve;
        result.issue = peakCurve.issue;
        return result;
    }
    result.clipPeakCurve = peakCurve.curve;

    const auto combined = combineStaticGainCurves(
        result.clipGainCurve, result.clipPeakCurve,
        snapshot.totalFrames(), config.combination);
    if (combined.status != CurveCombinationStatus::Ready) {
        result.status = AnalysisPipelineStatus::CombinationFailed;
        result.issue = combined.issue;
        return result;
    }

    result.combinedCurve = combined.curve;
    result.judges = runFiveJudges(
        result.phraseProfiles.profiles,
        result.reference,
        result.clipGainRecommendations.recommendations,
        result.clipPeakEvents,
        result.combinedCurve,
        snapshot.totalFrames(),
        config.judges);
    result.status = AnalysisPipelineStatus::Ready;
    return result;
}

} // namespace pgclip
