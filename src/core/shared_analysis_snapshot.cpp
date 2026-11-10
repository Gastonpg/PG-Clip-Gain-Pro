#include "pgclip/core/shared_analysis_snapshot.hpp"

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

    if (analysis.windows.front().range.start != 0U ||
        analysis.windows.back().range.end != capture.totalFrames) {
        return {
            SnapshotBuildStatus::CoverageMismatch,
            {ValidationCode::RangeOutsideCapture,
             "Window descriptors do not cover the full captured region."},
            {}
        };
    }

    FrameIndex previousStart = 0U;
    FrameIndex previousEnd = 0U;
    bool first = true;

    for (const auto& window : analysis.windows) {
        if (const auto rangeIssue = validate(window.range, capture.totalFrames)) {
            return {SnapshotBuildStatus::CoverageMismatch, rangeIssue, {}};
        }

        if ((!first && window.range.start <= previousStart) ||
            (!first && window.range.start > previousEnd) ||
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
            if (!std::isfinite(channel.rms) ||
                !std::isfinite(channel.peak) ||
                channel.rms < 0.0 || channel.peak < 0.0 ||
                channel.rms > channel.peak ||
                (channel.silent != (channel.peak == 0.0))) {
                return {
                    SnapshotBuildStatus::InvalidDescriptor,
                    {ValidationCode::NonFiniteGain,
                     "Window descriptor contains invalid RMS, peak, or silence state."},
                    {}
                };
            }
        }

        previousStart = window.range.start;
        previousEnd = window.range.end;
        first = false;
    }

    auto snapshot = std::shared_ptr<const SharedAnalysisSnapshot>(
        new SharedAnalysisSnapshot(
            capture.format, capture.totalFrames, config,
            std::move(analysis.windows)));

    return {SnapshotBuildStatus::Ready, {}, std::move(snapshot)};
}

} // namespace pgclip
