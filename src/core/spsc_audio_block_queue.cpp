#include "pgclip/core/spsc_audio_block_queue.hpp"

#include <algorithm>
#include <limits>

namespace pgclip {

SpscAudioBlockQueue::SpscAudioBlockQueue(
    std::uint32_t slotCount,
    std::uint32_t maxFramesPerBlock,
    std::uint32_t maxChannels)
    : slotCount_(slotCount),
      maxFramesPerBlock_(maxFramesPerBlock),
      maxChannels_(maxChannels) {
    if (slotCount < 2U || maxFramesPerBlock == 0U ||
        maxChannels == 0U || maxChannels > 64U) {
        slotCount_ = 0U;
        return;
    }

    const auto frames = static_cast<std::size_t>(maxFramesPerBlock);
    const auto channels = static_cast<std::size_t>(maxChannels);
    if (frames > std::numeric_limits<std::size_t>::max() / channels) {
        slotCount_ = 0U;
        return;
    }

    samplesPerSlot_ = frames * channels;
    const auto slots = static_cast<std::size_t>(slotCount);
    if (samplesPerSlot_ >
        std::numeric_limits<std::size_t>::max() / slots) {
        slotCount_ = 0U;
        samplesPerSlot_ = 0U;
        return;
    }

    sampleStorage_.resize(samplesPerSlot_ * slots);
    slotInfo_.resize(slots);
}

AudioQueueResult SpscAudioBlockQueue::tryPush(
    const AudioBlockView& block) noexcept {
    if (!isInitialized()) {
        return AudioQueueResult::NotInitialized;
    }

    if (block.channelData == nullptr ||
        block.frameCount == 0U ||
        block.frameCount > maxFramesPerBlock_ ||
        block.channelCount == 0U ||
        block.channelCount > maxChannels_ ||
        block.sourceStartFrame >
            std::numeric_limits<std::uint64_t>::max() - block.frameCount ||
        (block.sampleFormat != AudioSampleFormat::Float32 &&
         block.sampleFormat != AudioSampleFormat::Float64)) {
        return AudioQueueResult::InvalidBlock;
    }

    for (std::uint32_t channel = 0U;
         channel < block.channelCount; ++channel) {
        const bool silent =
            (block.silenceFlags & (std::uint64_t{1} << channel)) != 0U;
        if (!silent && block.channelData[channel] == nullptr) {
            return AudioQueueResult::InvalidBlock;
        }
    }

    const auto write = writeIndex_.load(std::memory_order_relaxed);
    const auto read = readIndex_.load(std::memory_order_acquire);
    if (write - read >= slotCount_) {
        droppedBlocks_.fetch_add(1U, std::memory_order_relaxed);
        return AudioQueueResult::Full;
    }

    const auto slot = static_cast<std::size_t>(write % slotCount_);
    auto* destination = sampleStorage_.data() + slot * samplesPerSlot_;

    for (std::uint32_t frame = 0U; frame < block.frameCount; ++frame) {
        for (std::uint32_t channel = 0U;
             channel < block.channelCount; ++channel) {
            const auto outputIndex =
                static_cast<std::size_t>(frame) * block.channelCount + channel;
            const bool silent =
                (block.silenceFlags & (std::uint64_t{1} << channel)) != 0U;

            if (silent) {
                destination[outputIndex] = 0.0F;
            } else if (block.sampleFormat == AudioSampleFormat::Float32) {
                const auto* source =
                    static_cast<const float*>(block.channelData[channel]);
                destination[outputIndex] = source[frame];
            } else {
                const auto* source =
                    static_cast<const double*>(block.channelData[channel]);
                destination[outputIndex] = static_cast<float>(source[frame]);
            }
        }
    }

    slotInfo_[slot] = {
        block.sourceStartFrame,
        block.frameCount,
        block.channelCount
    };

    writeIndex_.store(write + 1U, std::memory_order_release);
    return AudioQueueResult::Success;
}

AudioQueueResult SpscAudioBlockQueue::tryPop(
    float* destination,
    std::size_t destinationCapacitySamples,
    QueuedAudioBlockInfo& info) noexcept {
    if (!isInitialized()) {
        return AudioQueueResult::NotInitialized;
    }

    const auto read = readIndex_.load(std::memory_order_relaxed);
    const auto write = writeIndex_.load(std::memory_order_acquire);
    if (read == write) {
        return AudioQueueResult::Empty;
    }

    const auto slot = static_cast<std::size_t>(read % slotCount_);
    const auto slotInfo = slotInfo_[slot];
    const auto required =
        static_cast<std::size_t>(slotInfo.frameCount) * slotInfo.channelCount;

    if (destination == nullptr || destinationCapacitySamples < required) {
        return AudioQueueResult::DestinationTooSmall;
    }

    const auto* source = sampleStorage_.data() + slot * samplesPerSlot_;
    std::copy_n(source, required, destination);

    info.sequence = read;
    info.sourceStartFrame = slotInfo.sourceStartFrame;
    info.frameCount = slotInfo.frameCount;
    info.channelCount = slotInfo.channelCount;

    readIndex_.store(read + 1U, std::memory_order_release);
    return AudioQueueResult::Success;
}

bool SpscAudioBlockQueue::isInitialized() const noexcept {
    return slotCount_ >= 2U &&
           samplesPerSlot_ > 0U &&
           sampleStorage_.size() ==
               samplesPerSlot_ * static_cast<std::size_t>(slotCount_) &&
           slotInfo_.size() == static_cast<std::size_t>(slotCount_);
}

std::uint64_t SpscAudioBlockQueue::droppedBlocks() const noexcept {
    return droppedBlocks_.load(std::memory_order_relaxed);
}

std::uint64_t SpscAudioBlockQueue::queuedBlocks() const noexcept {
    const auto write = writeIndex_.load(std::memory_order_acquire);
    const auto read = readIndex_.load(std::memory_order_acquire);
    const auto count = write - read;
    return count <= slotCount_ ? count : 0U;
}

std::size_t SpscAudioBlockQueue::requiredSamplesPerSlot() const noexcept {
    return samplesPerSlot_;
}

} // namespace pgclip
