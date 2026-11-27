#pragma once

#include "pgclip/core/clip_gain_foundation.hpp"
#include "pgclip/core/clip_peak_foundation.hpp"
#include "pgclip/core/gain_curve_processor.hpp"

namespace pgclip {

struct FiveJudgeConfig {
    double minimumApprovalScore{0.55};
    double maximumCorrectionDb{6.0};
    double maximumAdjacentChangeDb{6.0};
};

struct JudgeOpinion {
    double score{0.0};
    bool veto{false};
};

struct FiveJudgeResult {
    bool evaluated{false};
    bool approved{false};
    double confidence{0.0};
    JudgeOpinion musicalInterpretation{};
    JudgeOpinion adaptiveLeveling{};
    JudgeOpinion transientProtection{};
    JudgeOpinion globalCoherence{};
    JudgeOpinion finalArbiter{};
    GainCurve approvedCurve{};
};

// Five deterministic specialist evaluators. These are transparent, testable
// decision agents, not a trained neural network. They never process or reshape
// audio; the arbiter approves or rejects the static Clip Gain/Clip Peak curve.
FiveJudgeResult runFiveJudges(
    const std::vector<PhraseProfile>& phrases,
    const AdaptiveReferenceResult& reference,
    const std::vector<PhraseGainRecommendation>& gainRecommendations,
    const ClipPeakResult& peakEvents,
    const GainCurve& proposedCurve,
    FrameIndex totalFrames,
    const FiveJudgeConfig& config = {});

} // namespace pgclip
