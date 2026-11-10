#include "pgclip/core/spsc_audio_block_queue.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

float expectedValue(std::uint64_t sequence,
                    std::uint32_t channel,
                    std::uint32_t frame) {
    return static_cast<float>(sequence * 0.01 +
                              channel * 0.25 +
                              frame * 0.001);
}
}

int main() {
    using namespace pgclip;

    SpscAudioBlockQueue invalidQueue(1U, 64U, 2U);
    check(!invalidQueue.isInitialized(),
          "reject queue with fewer than two slots");

    SpscAudioBlockQueue queue(2U, 8U, 2U);
    check(queue.isInitialized(), "initialize bounded queue");
    check(queue.requiredSamplesPerSlot() == 16U,
          "preallocate fixed slot capacity");

    std::array<float, 4> left{0.1F, 0.2F, 0.3F, 0.4F};
    std::array<float, 4> right{0.5F, 0.6F, 0.7F, 0.8F};
    const std::array<const void*, 2> channels{left.data(), right.data()};

    AudioBlockView block{
        channels.data(), 120U, 4U, 2U,
        AudioSampleFormat::Float32, 0U
    };
    check(queue.tryPush(block) == AudioQueueResult::Success,
          "enqueue planar float32 audio");
    check(queue.tryPush(block) == AudioQueueResult::Success,
          "fill second queue slot");
    check(queue.tryPush(block) == AudioQueueResult::Full,
          "report full queue without blocking");
    check(queue.droppedBlocks() == 1U,
          "count dropped block when bounded queue is full");

    std::array<float, 8> output{};
    QueuedAudioBlockInfo info{};
    check(queue.tryPop(output.data(), output.size(), info) ==
              AudioQueueResult::Success,
          "dequeue first audio block");
    check(info.sequence == 0U && info.sourceStartFrame == 120U &&
              info.frameCount == 4U && info.channelCount == 2U,
          "preserve block timing and metadata");
    check(std::abs(output[0] - 0.1F) < 1.0e-6F &&
              std::abs(output[1] - 0.5F) < 1.0e-6F &&
              std::abs(output[6] - 0.4F) < 1.0e-6F &&
              std::abs(output[7] - 0.8F) < 1.0e-6F,
          "interleave planar channels in deterministic order");

    check(queue.tryPop(output.data(), 2U, info) ==
              AudioQueueResult::DestinationTooSmall,
          "do not consume when worker destination is too small");
    check(queue.queuedBlocks() == 1U,
          "keep block queued after insufficient destination");
    check(queue.tryPop(output.data(), output.size(), info) ==
              AudioQueueResult::Success,
          "consume block after retry with sufficient capacity");

    const std::array<const void*, 2> oneSilentChannel{
        nullptr, right.data()
    };
    AudioBlockView silentBlock{
        oneSilentChannel.data(), 200U, 4U, 2U,
        AudioSampleFormat::Float32, 0x1U
    };
    check(queue.tryPush(silentBlock) == AudioQueueResult::Success,
          "accept null pointer for channel explicitly flagged silent");
    check(queue.tryPop(output.data(), output.size(), info) ==
              AudioQueueResult::Success,
          "dequeue block with silent channel");
    check(output[0] == 0.0F && output[2] == 0.0F &&
              std::abs(output[1] - 0.5F) < 1.0e-6F,
          "write exact zeros for flagged silent channels");

    std::array<double, 3> doubleLeft{0.125, -0.25, 0.5};
    std::array<double, 3> doubleRight{-0.75, 0.875, 1.0};
    const std::array<const void*, 2> doubleChannels{
        doubleLeft.data(), doubleRight.data()
    };
    AudioBlockView doubleBlock{
        doubleChannels.data(), 300U, 3U, 2U,
        AudioSampleFormat::Float64, 0U
    };
    check(queue.tryPush(doubleBlock) == AudioQueueResult::Success,
          "accept planar float64 audio");
    check(queue.tryPop(output.data(), output.size(), info) ==
              AudioQueueResult::Success,
          "dequeue converted float64 block");
    check(std::abs(output[0] - 0.125F) < 1.0e-6F &&
              std::abs(output[1] + 0.75F) < 1.0e-6F &&
              std::abs(output[5] - 1.0F) < 1.0e-6F,
          "convert float64 analysis copy to float32");

    AudioBlockView badBlock = block;
    badBlock.frameCount = 0U;
    check(queue.tryPush(badBlock) == AudioQueueResult::InvalidBlock,
          "reject zero-frame blocks");
    badBlock = block;
    badBlock.sourceStartFrame = UINT64_MAX - 1U;
    check(queue.tryPush(badBlock) == AudioQueueResult::InvalidBlock,
          "reject source timeline overflow");

    // Stress the SPSC memory-order contract with one producer and one consumer.
    constexpr std::uint64_t totalBlocks = 20000U;
    SpscAudioBlockQueue stressQueue(8U, 32U, 2U);
    std::atomic<bool> stressFailed{false};

    std::thread producer([&]() {
        std::uint64_t sourceFrame = 0U;
        for (std::uint64_t sequence = 0U;
             sequence < totalBlocks; ++sequence) {
            const auto frames =
                static_cast<std::uint32_t>((sequence % 32U) + 1U);
            std::array<std::vector<float>, 2> samples{
                std::vector<float>(frames),
                std::vector<float>(frames)
            };

            for (std::uint32_t frame = 0U; frame < frames; ++frame) {
                samples[0][frame] = expectedValue(sequence, 0U, frame);
                samples[1][frame] = expectedValue(sequence, 1U, frame);
            }

            const std::array<const void*, 2> source{
                samples[0].data(), samples[1].data()
            };
            const AudioBlockView current{
                source.data(), sourceFrame, frames, 2U,
                AudioSampleFormat::Float32, 0U
            };

            while (stressQueue.tryPush(current) == AudioQueueResult::Full) {
                std::this_thread::yield();
            }
            if (stressQueue.queuedBlocks() > 8U) {
                stressFailed.store(true, std::memory_order_relaxed);
            }
            sourceFrame += frames;
        }
    });

    std::thread consumer([&]() {
        std::uint64_t expectedSequence = 0U;
        std::uint64_t expectedSourceFrame = 0U;
        std::array<float, 64> destination{};
        while (expectedSequence < totalBlocks) {
            QueuedAudioBlockInfo current{};
            const auto result = stressQueue.tryPop(
                destination.data(), destination.size(), current);
            if (result == AudioQueueResult::Empty) {
                std::this_thread::yield();
                continue;
            }
            if (result != AudioQueueResult::Success ||
                current.sequence != expectedSequence ||
                current.sourceStartFrame != expectedSourceFrame ||
                current.channelCount != 2U) {
                stressFailed.store(true, std::memory_order_relaxed);
                break;
            }

            for (std::uint32_t frame = 0U;
                 frame < current.frameCount; ++frame) {
                const auto leftIndex = static_cast<std::size_t>(frame) * 2U;
                if (std::abs(destination[leftIndex] -
                             expectedValue(expectedSequence, 0U, frame)) > 1.0e-5F ||
                    std::abs(destination[leftIndex + 1U] -
                             expectedValue(expectedSequence, 1U, frame)) > 1.0e-5F) {
                    stressFailed.store(true, std::memory_order_relaxed);
                }
            }

            expectedSourceFrame += current.frameCount;
            ++expectedSequence;
        }
    });

    producer.join();
    consumer.join();
    check(!stressFailed.load(std::memory_order_relaxed),
          "preserve sequence, timing and samples under SPSC stress");
    check(stressQueue.queuedBlocks() == 0U,
          "drain queue after producer and consumer finish");

    if (failures != 0) {
        std::cerr << failures << " SPSC queue test(s) failed.\n";
        return 1;
    }

    std::cout << "All SPSC audio queue tests passed.\n";
    return 0;
}
