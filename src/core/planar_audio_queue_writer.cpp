#include "pgclip/core/planar_audio_queue_writer.hpp"

#include <limits>

namespace pgclip {

AudioQueueWriteReport pushPlanarAudioInChunks(
    SpscAudioBlockQueue& queue,
    const void* const* channelData,
    std::uint32_t channelCount,
    std::uint32_t frameCount,
    std::uint32_t maxFramesPerChunk,
    std::uint64_t sourceStartFrame,
    AudioSampleFormat sampleFormat,
    std::uint64_t silenceFlags) noexcept {
    AudioQueueWriteReport report{};

    if (channelData == nullptr || channelCount == 0U ||
        channelCount > 64U || frameCount == 0U ||
        maxFramesPerChunk == 0U ||
        sourceStartFrame > std::numeric_limits<std::uint64_t>::max() - frameCount ||
        (sampleFormat != AudioSampleFormat::Float32 &&
         sampleFormat != AudioSampleFormat::Float64)) {
        report.result = AudioQueueResult::InvalidBlock;
        return report;
    }

    for (std::uint32_t channel = 0U; channel < channelCount; ++channel) {
        const bool silent =
            (silenceFlags & (std::uint64_t{1} << channel)) != 0U;
        if (!silent && channelData[channel] == nullptr) {
            report.result = AudioQueueResult::InvalidBlock;
            return report;
        }
    }

    std::uint32_t offset = 0U;
    while (offset < frameCount) {
        const auto remaining = frameCount - offset;
        const auto chunkFrames =
            remaining < maxFramesPerChunk ? remaining : maxFramesPerChunk;
        const auto byteStride = sampleFormat == AudioSampleFormat::Float32
            ? sizeof(float) : sizeof(double);

        const void* chunkChannels[64]{};
        for (std::uint32_t channel = 0U; channel < channelCount; ++channel) {
            if (channelData[channel] != nullptr) {
                const auto* bytes =
                    static_cast<const unsigned char*>(channelData[channel]);
                chunkChannels[channel] = bytes +
                    static_cast<std::size_t>(offset) * byteStride;
            }
        }

        const AudioBlockView block{
            chunkChannels,
            sourceStartFrame + offset,
            chunkFrames,
            channelCount,
            sampleFormat,
            silenceFlags
        };
        report.result = queue.tryPush(block);
        if (report.result != AudioQueueResult::Success) {
            return report;
        }

        ++report.acceptedBlocks;
        report.acceptedFrames += chunkFrames;
        offset += chunkFrames;
    }

    report.complete = true;
    return report;
}

} // namespace pgclip
