#pragma once

#include "pgclip/core/clip_gain_foundation.hpp"
#include "pgclip/core/clip_peak_foundation.hpp"
#include "pgclip/core/gain_curve_processor.hpp"
#include "pgclip/core/five_judges.hpp"

namespace pgclip {

struct AnalysisPipelineConfig {
    ActivityDetectionConfig activity{};
    PhraseProfileConfig phraseProfiles{};
    AdaptiveReferenceConfig adaptiveReference{};
    ClipGainRecommendationConfig clipGain{};
    ClipGainCurveConfig clipGainCurve{};
    ClipPeakConfig clipPeak{};
    ClipPeakCurveConfig clipPeakCurve{};
    CurveCombinationConfig combination{};
    FiveJudgeConfig judges{};
};

enum class AnalysisPipelineStatus {
    Ready,
    InvalidActivity,
    NoReliableActivity,
    InvalidPhraseProfiles,
    ReferenceUnavailable,
    InvalidClipGain,
    InvalidClipPeak,
    InvalidClipGainCurve,
    InvalidClipPeakCurve,
    CombinationFailed
};

// Both engines receive the exact same immutable snapshot and activity map.
// Their proposals remain independent until the final safety combination.
struct AnalysisPipelineResult {
    AnalysisPipelineStatus status{AnalysisPipelineStatus::InvalidActivity};
    ValidationIssue issue{};
    ActivityDetectionResult activity{};
    PhraseProfileResult phraseProfiles{};
    AdaptiveReferenceResult reference{};
    ClipGainRecommendationResult clipGainRecommendations{};
    ClipPeakResult clipPeakEvents{};
    GainCurve clipGainCurve{};
    GainCurve clipPeakCurve{};
    GainCurve combinedCurve{};
    FiveJudgeResult judges{};
};

AnalysisPipelineResult buildAnalysisPipeline(
    const SharedAnalysisSnapshot& snapshot,
    const AnalysisPipelineConfig& config);

} // namespace pgclip
