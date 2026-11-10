#include "pgclip/core/window_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace pgclip {

ValidationIssue validate(const AudioBufferView& audio) {
    if (const auto formatIssue = validate(audio.format)) {
        return formatIssue;
    }

    if (audio.frameCount == 0U || audio.interleaved == nullptr) {
        return {ValidationCode::EmptyRange,
                "Audio view must contain frames and a valid sample pointer."};
    }

    const auto channels = static_cast<std::size_t>(audio.format.channelCount);
    const auto maxFrames =
        std::numeric_limits<std::size_t>::max() / channels;
    if (audio.frameCount > static_cast<FrameIndex>(maxFrames)) {
        return {ValidationCode::RangeOutsideCapture,
                "Audio view exceeds addressable interleaved sample storage."};
    }

    return {};
}

WindowAnalysisResult analyzeWindowDescriptors(
    const AudioBufferView& audio,
    const AnalysisConfig& config) {
    if (const auto audioIssue = validate(audio)) {
        return {WindowAnalysisStatus::InvalidAudio, audioIssue, {}};
    }

    if (const auto configIssue = validate(config)) {
        return {WindowAnalysisStatus::InvalidConfiguration, configIssue, {}};
    }

    const FrameIndex windowCount =
        ((audio.frameCount - 1U) / config.hopFrames) + 1U;
    if (windowCount >
        static_cast<FrameIndex>(std::vector<WindowDescriptor>{}.max_size())) {
        return {
            WindowAnalysisStatus::InvalidAudio,
            {ValidationCode::RangeOutsideCapture,
             "Window count exceeds addressable descriptor storage."},
            {}
        };
    }

    WindowAnalysisResult result;
    result.status = WindowAnalysisStatus::Success;
    result.windows.reserve(static_cast<std::size_t>(windowCount));

    const auto channelCount =
        static_cast<std::size_t>(audio.format.channelCount);

    for (FrameIndex start = 0U; start < audio.frameCount;) {
        const FrameIndex available = audio.frameCount - start;
        const FrameIndex length = std::min(config.windowFrames, available);
        const FrameIndex end = start + length;

        WindowDescriptor descriptor;
        descriptor.range = {start, end};
        descriptor.channels.resize(channelCount);

        for (std::size_t channel = 0U; channel < channelCount; ++channel) {
            double sumSquares = 0.0;
            double peak = 0.0;
            bool finite = true;

            for (FrameIndex frame = start; frame < end; ++frame) {
                const auto sampleIndex =
                    static_cast<std::size_t>(frame) * channelCount + channel;
                const double sample =
                    static_cast<double>(audio.interleaved[sampleIndex]);

                if (!std::isfinite(sample)) {
                    finite = false;
                    break;
                }

                sumSquares += sample * sample;
                peak = std::max(peak, std::abs(sample));
            }

            auto& metrics = descriptor.channels[channel];
            metrics.finite = finite;
            if (finite) {
                metrics.rms = std::sqrt(
                    sumSquares / static_cast<double>(length));
                metrics.peak = peak;
                metrics.silent = peak == 0.0;
            } else {
                metrics.rms = 0.0;
                metrics.peak = 0.0;
                metrics.silent = false;
            }
        }

        result.windows.push_back(std::move(descriptor));

        if (config.hopFrames >= available) {
            break;
        }
        start += config.hopFrames;
    }

    return result;
}

} // namespace pgclip
