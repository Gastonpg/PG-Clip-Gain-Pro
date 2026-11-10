#pragma once

#include "pgclip/core/types.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace pgclip::test {

struct PcmBuffer {
    AudioFormat format{};
    FrameIndex frameCount{0};
    std::vector<float> interleaved;
};

inline std::optional<PcmBuffer> makeSilence(AudioFormat format,
                                            FrameIndex frameCount) {
    if (validate(format) || frameCount == 0U) {
        return std::nullopt;
    }

    const auto channels = static_cast<std::size_t>(format.channelCount);
    if (frameCount > static_cast<FrameIndex>(
            std::vector<float>{}.max_size() / channels)) {
        return std::nullopt;
    }

    PcmBuffer result{format, frameCount, {}};
    try {
        result.interleaved.assign(
            static_cast<std::size_t>(frameCount) * channels, 0.0F);
    } catch (...) {
        return std::nullopt;
    }
    return result;
}

inline std::optional<PcmBuffer> makeSine(AudioFormat format,
                                         FrameIndex frameCount,
                                         double frequencyHz,
                                         double amplitude,
                                         bool invertRightChannel = false) {
    if (validate(format) || frameCount == 0U ||
        !std::isfinite(frequencyHz) || !std::isfinite(amplitude) ||
        frequencyHz <= 0.0 ||
        frequencyHz >= static_cast<double>(format.sampleRate) / 2.0 ||
        amplitude < 0.0 || amplitude > 1.0) {
        return std::nullopt;
    }

    auto result = makeSilence(format, frameCount);
    if (!result) {
        return std::nullopt;
    }

    constexpr double twoPi = 6.283185307179586476925286766559;
    const auto channels = static_cast<std::size_t>(format.channelCount);
    for (FrameIndex frame = 0; frame < frameCount; ++frame) {
        const double time = static_cast<double>(frame) /
                            static_cast<double>(format.sampleRate);
        const float sample = static_cast<float>(
            amplitude * std::sin(twoPi * frequencyHz * time));

        const auto base = static_cast<std::size_t>(frame) * channels;
        result->interleaved[base] = sample;
        if (channels == 2U) {
            result->interleaved[base + 1U] =
                invertRightChannel ? -sample : sample;
        }
    }

    return result;
}

inline std::optional<PcmBuffer> makeImpulse(AudioFormat format,
                                            FrameIndex frameCount,
                                            FrameIndex impulseFrame,
                                            float amplitude) {
    if (validate(format) || frameCount == 0U ||
        impulseFrame >= frameCount || !std::isfinite(amplitude) ||
        amplitude < -1.0F || amplitude > 1.0F) {
        return std::nullopt;
    }

    auto result = makeSilence(format, frameCount);
    if (!result) {
        return std::nullopt;
    }

    const auto channels = static_cast<std::size_t>(format.channelCount);
    const auto base = static_cast<std::size_t>(impulseFrame) * channels;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        result->interleaved[base + channel] = amplitude;
    }
    return result;
}

} // namespace pgclip::test
