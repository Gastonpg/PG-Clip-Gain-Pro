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
    result.format = audio.format;
    result.totalFrames = audio.frameCount;
    result.config = config;
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
            double sumAbsolute = 0.0;
            double sumSamples = 0.0;
            double peak = 0.0;
            std::uint64_t zeroCrossings = 0U;
            int previousSign = 0;
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

                const double absolute = std::abs(sample);
                sumSquares += sample * sample;
                sumAbsolute += absolute;
                sumSamples += sample;
                peak = std::max(peak, absolute);

                const int sign = sample > 0.0 ? 1 : (sample < 0.0 ? -1 : 0);
                if (sign != 0) {
                    if (previousSign != 0 && sign != previousSign) {
                        ++zeroCrossings;
                    }
                    previousSign = sign;
                }
            }

            auto& metrics = descriptor.channels[channel];
            metrics.finite = finite;
            if (finite) {
                const double frameCount = static_cast<double>(length);
                metrics.rms = std::sqrt(sumSquares / frameCount);
                metrics.peak = peak;
                metrics.meanAbsolute = sumAbsolute / frameCount;
                metrics.dcOffset = sumSamples / frameCount;
                metrics.zeroCrossingRate = length > 1U
                    ? static_cast<double>(zeroCrossings) /
                        static_cast<double>(length - 1U)
                    : 0.0;
                metrics.crestFactor = metrics.rms > 0.0
                    ? metrics.peak / metrics.rms
                    : 0.0;
                metrics.silent = peak == 0.0;
            } else {
                metrics.rms = 0.0;
                metrics.peak = 0.0;
                metrics.meanAbsolute = 0.0;
                metrics.dcOffset = 0.0;
                metrics.zeroCrossingRate = 0.0;
                metrics.crestFactor = 0.0;
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
