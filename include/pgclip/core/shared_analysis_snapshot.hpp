#pragma once

#include "pgclip/core/window_analysis.hpp"

#include <memory>
#include <vector>

namespace pgclip {

enum class SnapshotBuildStatus {
    Ready,
    IncompleteCapture,
    InvalidAnalysis,
    CoverageMismatch,
    InvalidDescriptor
};

class SharedAnalysisSnapshot {
public:
    std::uint32_t version() const noexcept;
    AudioFormat format() const noexcept;
    FrameIndex totalFrames() const noexcept;
    AnalysisConfig config() const noexcept;
    const std::vector<WindowDescriptor>& windows() const noexcept;

private:
    friend struct SnapshotBuildResult;
    friend SnapshotBuildResult buildSharedAnalysisSnapshot(
        const CaptureDescriptor&, const AnalysisConfig&,
        WindowAnalysisResult);

    SharedAnalysisSnapshot(AudioFormat format,
                           FrameIndex totalFrames,
                           AnalysisConfig config,
                           std::vector<WindowDescriptor> windows);

    std::uint32_t version_{1};
    AudioFormat format_{};
    FrameIndex totalFrames_{0};
    AnalysisConfig config_{};
    std::vector<WindowDescriptor> windows_;
};

struct SnapshotBuildResult {
    SnapshotBuildStatus status{SnapshotBuildStatus::InvalidAnalysis};
    ValidationIssue issue{};
    std::shared_ptr<const SharedAnalysisSnapshot> snapshot;
};

SnapshotBuildResult buildSharedAnalysisSnapshot(
    const CaptureDescriptor& capture,
    const AnalysisConfig& config,
    WindowAnalysisResult analysis);

} // namespace pgclip
