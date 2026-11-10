#pragma once

#include "pgclip/core/shared_analysis_snapshot.hpp"
#include "pgclip/core/window_activity.hpp"

#include <cstdint>
#include <vector>

namespace pgclip {

struct PhraseProfileConfig {
    std::uint32_t minimumActiveWindows{0};
    double minimumConfidence{0.0};
};

struct PhraseProfile {
    FrameRange range{};
    double rmsLevelDb{0.0}; // RMS energy proxy, not final perceived loudness.
    double peakDb{0.0};
    double meanAbsoluteDb{0.0};
    double crestFactor{0.0};
    std::uint32_t activeWindowCount{0};
    double confidence{0.0};
    bool eligibleForReference{false};
};

enum class PhraseProfileStatus {
    Ready,
    NoCandidateRegions,
    InvalidConfiguration
};

struct PhraseProfileResult {
    PhraseProfileStatus status{PhraseProfileStatus::InvalidConfiguration};
    ValidationIssue issue{};
    std::vector<PhraseProfile> profiles;
};

PhraseProfileResult analyzePhraseProfiles(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionResult& activity,
    const PhraseProfileConfig& config);

struct AdaptiveReferenceConfig {
    std::uint32_t minimumPhraseCount{0};
    double quantile{0.0};
};

enum class AdaptiveReferenceStatus {
    Ready,
    NotEnoughPhrases,
    InvalidConfiguration
};

struct AdaptiveReferenceResult {
    AdaptiveReferenceStatus status{AdaptiveReferenceStatus::InvalidConfiguration};
    ValidationIssue issue{};
    double referenceRmsDb{0.0}; // Adaptive reference in RMS-proxy dB.
    double medianAbsoluteDeviationDb{0.0};
    std::uint32_t eligiblePhraseCount{0};
};

AdaptiveReferenceResult selectAdaptiveReference(
    const std::vector<PhraseProfile>& profiles,
    const AdaptiveReferenceConfig& config);

struct ClipGainRecommendationConfig {
    double correctionStrength{0.0};
    double maximumBoostDb{0.0};
    double maximumCutDb{0.0};
    double minimumConfidence{0.0};
};

enum class GainRecommendationAction {
    Adjust,
    Keep,
    Abstain
};

enum class ClipGainRecommendationStatus {
    Ready,
    ReferenceUnavailable,
    InvalidConfiguration
};

struct PhraseGainRecommendation {
    FrameRange range{};
    double correctionDb{0.0};
    double confidence{0.0};
    GainRecommendationAction action{GainRecommendationAction::Abstain};
};

struct ClipGainRecommendationResult {
    ClipGainRecommendationStatus status{
        ClipGainRecommendationStatus::InvalidConfiguration};
    ValidationIssue issue{};
    std::vector<PhraseGainRecommendation> recommendations;
};

// Produces editable static phrase-level recommendations only.
// It does not process audio, compress, limit, or set an absolute target level.
ClipGainRecommendationResult recommendClipGain(
    const std::vector<PhraseProfile>& profiles,
    const AdaptiveReferenceResult& reference,
    const ClipGainRecommendationConfig& config);

} // namespace pgclip
