#include "pgclip/core/shared_analysis_snapshot.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace pgclip {

SharedAnalysisSnapshot::SharedAnalysisSnapshot(
    AudioFormat format,
    FrameIndex totalFrames,
    AnalysisConfig config,
    std::vector<WindowDescriptor> windows)
    : format_(format),
      totalFrames_(totalFrames),
      config_(config),
      windows_(std::move(windows)) {}

std::uint32_t SharedAnalysisSnapshot::version() const noexcept {
    return version_;
}

AudioFormat SharedAnalysisSnapshot::format() const noexcept {
    return format_;
}

FrameIndex SharedAnalysisSnapshot::totalFrames() const noexcept {
    return totalFrames_;
}

AnalysisConfig SharedAnalysisSnapshot::config() const noexcept {
    return config_;
}

const std::vector<WindowDescriptor>&
SharedAnalysisSnapshot::windows() const noexcept {
    return windows_;
}

SnapshotBuildResult buildSharedAnalysisSnapshot(
    const CaptureDescriptor& capture,
    const AnalysisConfig& config,
    WindowAnalysisResult analysis) {
    if (capture.state != CaptureState::Complete ||
        capture.hasDiscontinuities || capture.totalFrames == 0U) {
        return {
            SnapshotBuildStatus::IncompleteCapture,
            {ValidationCode::RangeOutsideCapture,
             "A shared full-region snapshot requires complete, continuous capture."},
            {}
        };
    }

    if (const auto captureIssue = validate(capture)) {
        return {SnapshotBuildStatus::IncompleteCapture, captureIssue, {}};
    }

    if (const auto configIssue = validate(config)) {
        return {SnapshotBuildStatus::InvalidAnalysis, configIssue, {}};
    }

    if (analysis.status != WindowAnalysisStatus::Success ||
        analysis.windows.empty()) {
        return {
            SnapshotBuildStatus::InvalidAnalysis,
            analysis.issue ? analysis.issue
                           : ValidationIssue{ValidationCode::InvalidWindow,
                                              "Window analysis did not produce descriptors."},
            {}
        };
    }

    if (analysis.config.windowFrames != config.windowFrames ||
        analysis.config.hopFrames != config.hopFrames ||
        analysis.config.version != config.version) {
        return {
            SnapshotBuildStatus::InvalidAnalysis,
            {ValidationCode::InvalidWindow,
             "Snapshot configuration does not match the configuration used for window analysis."},
            {}
        };
    }

    if (analysis.format.sampleRate != capture.format.sampleRate ||
        analysis.format.channelCount != capture.format.channelCount ||
        analysis.totalFrames != capture.totalFrames ||
        analysis.windows.front().range.start != 0U ||
        analysis.windows.back().range.end != capture.totalFrames) {
        return {
            SnapshotBuildStatus::CoverageMismatch,
            {ValidationCode::RangeOutsideCapture,
             "Window descriptors do not cover the full captured region."},
            {}
        };
    }

    const FrameIndex expectedWindowCount =
        ((capture.totalFrames - 1U) / config.hopFrames) + 1U;
    if (analysis.windows.size() !=
        static_cast<std::size_t>(expectedWindowCount)) {
        return {
            SnapshotBuildStatus::CoverageMismatch,
            {ValidationCode::RangeOutsideCapture,
             "Window descriptor count does not match the configured hop and capture length."},
            {}
        };
    }

    FrameIndex expectedStart = 0U;
    for (std::size_t windowIndex = 0U;
         windowIndex < analysis.windows.size(); ++windowIndex) {
        const auto& window = analysis.windows[windowIndex];
        if (expectedStart >= capture.totalFrames) {
            return {
                SnapshotBuildStatus::CoverageMismatch,
                {ValidationCode::RangeOutsideCapture,
                 "Window descriptors extend beyond the configured analysis grid."},
                {}
            };
        }

        const FrameIndex expectedLength =
            std::min(config.windowFrames, capture.totalFrames - expectedStart);
        const FrameIndex expectedEnd = expectedStart + expectedLength;

        if (const auto rangeIssue = validate(window.range, capture.totalFrames)) {
            return {SnapshotBuildStatus::CoverageMismatch, rangeIssue, {}};
        }

        if (window.range.start != expectedStart ||
            window.range.end != expectedEnd ||
            window.channels.size() !=
                static_cast<std::size_t>(capture.format.channelCount)) {
            return {
                SnapshotBuildStatus::CoverageMismatch,
                {ValidationCode::RangeOutsideCapture,
                 "Window order, overlap coverage, or channel count is inconsistent."},
                {}
            };
        }

        for (const auto& channel : window.channels) {
            const bool invalidNumbers =
                !std::isfinite(channel.rms) ||
                !std::isfinite(channel.peak) ||
                !std::isfinite(channel.meanAbsolute) ||
                !std::isfinite(channel.dcOffset) ||
                !std::isfinite(channel.zeroCrossingRate) ||
                !std::isfinite(channel.crestFactor) ||
                channel.rms < 0.0 || channel.peak < 0.0 ||
                channel.meanAbsolute < 0.0 ||
                channel.zeroCrossingRate < 0.0 ||
                channel.zeroCrossingRate > 1.0 ||
                channel.crestFactor < 0.0;

            const double metricTolerance =
                1.0e-12 * std::max(1.0, channel.peak);
            const bool inconsistentFiniteMetrics =
                channel.finite &&
                (channel.rms > channel.peak + metricTolerance ||
                 channel.meanAbsolute > channel.peak + metricTolerance ||
                 std::abs(channel.dcOffset) >
                     channel.meanAbsolute + metricTolerance ||
                 (channel.rms == 0.0 && channel.peak != 0.0) ||
                 (channel.rms > 0.0 &&
                  std::abs(channel.crestFactor -
                           channel.peak / channel.rms) >
                      1.0e-12 * std::max(1.0, channel.crestFactor)) ||
                 (channel.rms == 0.0 && channel.crestFactor != 0.0) ||
                 channel.silent != (channel.peak == 0.0));

            const bool inconsistentInvalidMetrics =
                !channel.finite &&
                (channel.rms != 0.0 || channel.peak != 0.0 ||
                 channel.meanAbsolute != 0.0 || channel.dcOffset != 0.0 ||
                 channel.zeroCrossingRate != 0.0 ||
                 channel.crestFactor != 0.0 || channel.silent);

            if (invalidNumbers || inconsistentFiniteMetrics ||
                inconsistentInvalidMetrics) {
                return {
                    SnapshotBuildStatus::InvalidDescriptor,
                    {ValidationCode::NonFiniteGain,
                     "Window descriptor contains invalid RMS, peak, or silence state."},
                    {}
                };
            }
        }

        const FrameIndex remaining = capture.totalFrames - expectedStart;
        if (windowIndex + 1U < analysis.windows.size()) {
            if (config.hopFrames >= remaining) {
                return {
                    SnapshotBuildStatus::CoverageMismatch,
                    {ValidationCode::RangeOutsideCapture,
                     "Unexpected extra window after the final analysis position."},
                    {}
                };
            }
            expectedStart += config.hopFrames;
        } else if (config.hopFrames < remaining) {
            return {
                SnapshotBuildStatus::CoverageMismatch,
                {ValidationCode::RangeOutsideCapture,
                 "Analysis grid ends before the captured region is fully covered."},
                {}
            };
        }
    }

    auto snapshot = std::shared_ptr<const SharedAnalysisSnapshot>(
        new SharedAnalysisSnapshot(
            capture.format, capture.totalFrames, config,
            std::move(analysis.windows)));

    return {SnapshotBuildStatus::Ready, {}, std::move(snapshot)};
}

} // namespace pgclip
