#include "pgclip/core/window_context.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace pgclip {
namespace {

double medianOfSorted(const std::vector<double>& sorted) {
    const auto count = sorted.size();
    const auto middle = count / 2U;

    if ((count % 2U) != 0U) {
        return sorted[middle];
    }

    const double lower = sorted[middle - 1U];
    const double upper = sorted[middle];
    return lower + (upper - lower) * 0.5;
}

std::optional<double> median(std::vector<double> values) {
    if (values.empty()) {
        return std::nullopt;
    }

    std::sort(values.begin(), values.end());
    return medianOfSorted(values);
}

} // namespace

WindowContextResult summarizeWindowContext(
    const SharedAnalysisSnapshot& snapshot) {
    const auto channelCount =
        static_cast<std::size_t>(snapshot.format().channelCount);

    WindowContextResult result;
    result.channels.resize(channelCount);

    std::vector<std::vector<double>> rmsValues(channelCount);
    std::vector<std::vector<double>> peakValues(channelCount);

    for (auto& channel : result.channels) {
        channel.totalWindows =
            static_cast<std::uint64_t>(snapshot.windows().size());
    }

    for (const auto& window : snapshot.windows()) {
        for (std::size_t channelIndex = 0U;
             channelIndex < channelCount; ++channelIndex) {
            auto& statistics = result.channels[channelIndex];
            const auto& metrics = window.channels[channelIndex];

            if (!metrics.finite) {
                ++statistics.invalidWindows;
                continue;
            }

            ++statistics.finiteWindows;
            if (metrics.silent) {
                ++statistics.silentWindows;
                continue;
            }

            ++statistics.nonSilentWindows;
            rmsValues[channelIndex].push_back(metrics.rms);
            peakValues[channelIndex].push_back(metrics.peak);
        }
    }

    bool hasAnyNonSilentChannel = false;

    for (std::size_t channelIndex = 0U;
         channelIndex < channelCount; ++channelIndex) {
        auto& statistics = result.channels[channelIndex];

        statistics.medianRms = median(rmsValues[channelIndex]);
        statistics.medianPeak = median(peakValues[channelIndex]);

        if (statistics.medianRms) {
            hasAnyNonSilentChannel = true;

            auto deviations = rmsValues[channelIndex];
            for (auto& value : deviations) {
                value = std::abs(value - *statistics.medianRms);
            }
            statistics.medianAbsoluteDeviationRms =
                median(std::move(deviations));

            const auto limits = std::minmax_element(
                rmsValues[channelIndex].begin(),
                rmsValues[channelIndex].end());
            statistics.minimumRms = *limits.first;
            statistics.maximumRms = *limits.second;
        }

        if (statistics.medianPeak) {
            const auto limits = std::minmax_element(
                peakValues[channelIndex].begin(),
                peakValues[channelIndex].end());
            statistics.minimumPeak = *limits.first;
            statistics.maximumPeak = *limits.second;
        }
    }

    result.status = hasAnyNonSilentChannel
        ? ContextAnalysisStatus::Ready
        : ContextAnalysisStatus::NoNonSilentWindows;

    return result;
}

} // namespace pgclip
