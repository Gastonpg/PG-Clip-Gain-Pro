#pragma once

#include "pgclip/core/types.hpp"

#include <vector>

namespace pgclip {

struct AudioBufferView {
    AudioFormat format{};
    FrameIndex frameCount{0};
    const float* interleaved{nullptr};
};

struct ChannelWindowMetrics {
    double rms{0.0};
    double peak{0.0};
    bool finite{true};
    bool silent{true};
};

struct WindowDescriptor {
    FrameRange range{};
    std::vector<ChannelWindowMetrics> channels;
};

enum class WindowAnalysisStatus {
    Success,
    InvalidAudio,
    InvalidConfiguration
};

struct WindowAnalysisResult {
    WindowAnalysisStatus status{WindowAnalysisStatus::InvalidAudio};
    ValidationIssue issue{};
    std::vector<WindowDescriptor> windows;
};

ValidationIssue validate(const AudioBufferView& audio);
WindowAnalysisResult analyzeWindowDescriptors(
    const AudioBufferView& audio,
    const AnalysisConfig& config);

} // namespace pgclip
