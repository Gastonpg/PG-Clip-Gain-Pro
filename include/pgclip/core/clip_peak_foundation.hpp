#pragma once

#include "pgclip/core/shared_analysis_snapshot.hpp"
#include "pgclip/core/window_activity.hpp"

#include <cstdint>
#include <vector>

namespace pgclip {

struct ClipPeakConfig {
    std::uint32_t localRadiusWindows{0};
    double peakExcessThresholdDb{0.0};
    double minimumCrestFactor{0.0};
    double maximumAttenuationDb{0.0};
    double minimumConfidence{0.0};
    std::uint32_t protectPhraseEdgeWindows{0};
};

enum class ClipPeakStatus {
    Ready,
    InvalidConfiguration,
    InvalidContext,
    NoUsableContext
};

enum class PeakEventAction {
    Review,
    Attenuate
};

struct PeakEvent {
    FrameRange range{};
    double peakDb{0.0};
    double localMedianPeakDb{0.0};
    double excessDb{0.0};
    double crestFactor{0.0};
    double suggestedAttenuationDb{0.0};
    double confidence{0.0};
    PeakEventAction action{PeakEventAction::Review};
    bool protectedPhraseEdge{false};
};

struct ClipPeakResult {
    ClipPeakStatus status{ClipPeakStatus::InvalidConfiguration};
    ValidationIssue issue{};
    std::uint64_t windowsScanned{0};
    std::uint64_t invalidWindows{0};
    std::vector<PeakEvent> events;
};

// Independent local-peak anomaly analysis. It proposes static attenuation
// events only; it does not compress, limit, clip or alter audio.
ClipPeakResult detectClipPeakEvents(
    const SharedAnalysisSnapshot& snapshot,
    const ActivityDetectionResult& activity,
    const ClipPeakConfig& config);

} // namespace pgclip
