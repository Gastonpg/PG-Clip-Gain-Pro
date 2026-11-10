#pragma once

#include "pgclip/core/shared_analysis_snapshot.hpp"

#include <cstdint>
#include <vector>

namespace pgclip {

struct ActivityDetectionConfig {
    double enterMarginDb{0.0};
    double exitMarginDb{0.0};
    double lowerQuantile{0.0};
    double minimumContrastDb{0.0};
    std::uint32_t bridgeGapWindows{0};
    std::uint32_t minimumActiveWindows{1};
};

enum class ActivityDetectionStatus {
    Ready,
    NoUsableWindows,
    InsufficientContrast,
    InvalidConfiguration
};

enum class WindowActivityState {
    Invalid,
    DigitalSilence,
    BelowThreshold,
    Active,
    BridgedPause,
    Uncertain
};

struct WindowActivity {
    FrameRange range{};
    double levelDb{0.0};
    double confidence{0.0};
    WindowActivityState state{WindowActivityState::Invalid};
};

struct ActivityRegionCandidate {
    FrameRange range{};
    std::uint32_t activeWindows{0};
    std::uint32_t bridgedPauseWindows{0};
    double confidence{0.0};
    bool containsBridgedPause{false};
};

struct ActivityDetectionResult {
    ActivityDetectionStatus status{ActivityDetectionStatus::InvalidConfiguration};
    ValidationIssue issue{};
    double noiseFloorDb{0.0};
    double enterThresholdDb{0.0};
    double exitThresholdDb{0.0};
    std::uint64_t validWindows{0};
    std::uint64_t invalidWindows{0};
    std::uint64_t silentWindows{0};
    std::vector<WindowActivity> windows;
    std::vector<ActivityRegionCandidate> regions;
};

// Produces conservative window-level activity candidates only.
// It does not claim speech recognition or final phrase boundaries.
ActivityDetectionResult detectWindowActivity(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionConfig& config);

} // namespace pgclip
