#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pgclip {

enum class AudioSampleFormat {
    Float32,
    Float64
};

struct AudioBlockView {
    const void* const* channelData{nullptr};
    std::uint64_t sourceStartFrame{0};
    std::uint32_t frameCount{0};
    std::uint32_t channelCount{0};
    AudioSampleFormat sampleFormat{AudioSampleFormat::Float32};
    std::uint64_t silenceFlags{0};
};

struct QueuedAudioBlockInfo {
    std::uint64_t sequence{0};
    std::uint64_t sourceStartFrame{0};
    std::uint32_t frameCount{0};
    std::uint32_t channelCount{0};
};

enum class AudioQueueResult {
    Success,
    Empty,
    Full,
    InvalidBlock,
    DestinationTooSmall,
    NotInitialized
};

// Strict single-producer/single-consumer queue.
// Construct and destroy it only while producer and consumer are stopped.
// tryPush/tryPop never allocate, lock, wait, or perform I/O.
class SpscAudioBlockQueue {
public:
    SpscAudioBlockQueue(std::uint32_t slotCount,
                        std::uint32_t maxFramesPerBlock,
                        std::uint32_t maxChannels);

    SpscAudioBlockQueue(const SpscAudioBlockQueue&) = delete;
    SpscAudioBlockQueue& operator=(const SpscAudioBlockQueue&) = delete;

    AudioQueueResult tryPush(const AudioBlockView& block) noexcept;
    AudioQueueResult tryPop(float* destination,
                            std::size_t destinationCapacitySamples,
                            QueuedAudioBlockInfo& info) noexcept;

    bool isInitialized() const noexcept;
    std::uint64_t droppedBlocks() const noexcept;
    std::uint64_t queuedBlocks() const noexcept;
    std::size_t requiredSamplesPerSlot() const noexcept;

private:
    struct SlotInfo {
        std::uint64_t sourceStartFrame{0};
        std::uint32_t frameCount{0};
        std::uint32_t channelCount{0};
    };

    std::uint32_t slotCount_{0};
    std::uint32_t maxFramesPerBlock_{0};
    std::uint32_t maxChannels_{0};
    std::size_t samplesPerSlot_{0};
    std::vector<float> sampleStorage_;
    std::vector<SlotInfo> slotInfo_;

    alignas(64) std::atomic<std::uint64_t> writeIndex_{0};
    alignas(64) std::atomic<std::uint64_t> readIndex_{0};
    std::atomic<std::uint64_t> droppedBlocks_{0};
};

} // namespace pgclip
