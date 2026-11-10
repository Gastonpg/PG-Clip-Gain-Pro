#pragma once

#include "pgclip/core/shared_analysis_snapshot.hpp"

#include <optional>
#include <vector>

namespace pgclip {

struct ChannelContextStatistics {
    std::uint64_t totalWindows{0};
    std::uint64_t finiteWindows{0};
    std::uint64_t invalidWindows{0};
    std::uint64_t silentWindows{0};
    std::uint64_t nonSilentWindows{0};

    std::optional<double> medianRms;
    std::optional<double> medianAbsoluteDeviationRms;
    std::optional<double> minimumRms;
    std::optional<double> maximumRms;

    std::optional<double> medianPeak;
    std::optional<double> minimumPeak;
    std::optional<double> maximumPeak;
};

enum class ContextAnalysisStatus {
    Ready,
    NoNonSilentWindows
};

struct WindowContextResult {
    ContextAnalysisStatus status{ContextAnalysisStatus::NoNonSilentWindows};
    std::vector<ChannelContextStatistics> channels;
};

// Descriptive statistics only. This does not classify voice, phrases, or intent.
WindowContextResult summarizeWindowContext(
    const SharedAnalysisSnapshot& snapshot);

} // namespace pgclip
