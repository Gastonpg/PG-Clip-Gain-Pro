#include "pgclip/core/planar_audio_queue_writer.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main() {
    using namespace pgclip;
    SpscAudioBlockQueue queue(8U, 4U, 2U);

    std::array<float, 10> left{};
    std::array<float, 10> right{};
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = static_cast<float>(i + 1U);
        right[i] = -static_cast<float>(i + 1U);
    }
    const void* channels[]{left.data(), right.data()};

    const auto report = pushPlanarAudioInChunks(
        queue, channels, 2U, 10U, 4U, 100U,
        AudioSampleFormat::Float32, 0U);
    check(report.complete, "oversized host block is fully split");
    check(report.acceptedBlocks == 3U, "three bounded chunks are enqueued");
    check(report.acceptedFrames == 10U, "all source frames are accounted");

    std::array<float, 8> output{};
    QueuedAudioBlockInfo info{};
    const std::uint64_t expectedStarts[]{100U, 104U, 108U};
    const std::uint32_t expectedFrames[]{4U, 4U, 2U};
    std::size_t expectedSample = 0U;
    for (std::size_t block = 0U; block < 3U; ++block) {
        check(queue.tryPop(output.data(), output.size(), info) ==
                  AudioQueueResult::Success,
              "consumer retrieves each split block");
        check(info.sourceStartFrame == expectedStarts[block],
              "split blocks preserve contiguous source positions");
        check(info.frameCount == expectedFrames[block],
              "last split block retains its shorter frame count");
        for (std::uint32_t frame = 0U; frame < info.frameCount; ++frame) {
            const auto index = static_cast<std::size_t>(frame) * 2U;
            check(std::fabs(output[index] -
                            left[expectedSample + frame]) < 1.0e-6F,
                  "left channel samples remain ordered");
            check(std::fabs(output[index + 1U] -
                            right[expectedSample + frame]) < 1.0e-6F,
                  "right channel samples remain ordered");
        }
        expectedSample += info.frameCount;
    }

    SpscAudioBlockQueue shortQueue(2U, 4U, 2U);
    const auto partial = pushPlanarAudioInChunks(
        shortQueue, channels, 2U, 10U, 4U, 200U,
        AudioSampleFormat::Float32, 0U);
    check(!partial.complete, "queue exhaustion reports partial enqueue");
    check(partial.result == AudioQueueResult::Full,
          "queue exhaustion reports full rather than success");
    check(partial.acceptedBlocks == 2U && partial.acceptedFrames == 8U,
          "partial enqueue reports exactly the accepted prefix");
    check(shortQueue.droppedBlocks() == 1U,
          "queue overflow marks capture as dropped");

    const void* invalidChannels[]{nullptr};
    const auto invalid = pushPlanarAudioInChunks(
        queue, invalidChannels, 1U, 4U, 4U, 0U,
        AudioSampleFormat::Float32, 0U);
    check(invalid.result == AudioQueueResult::InvalidBlock,
          "nonnull audio channel is required unless marked silent");

    if (failures != 0) {
        std::cerr << failures << " planar queue writer test(s) failed.\n";
        return 1;
    }
    std::cout << "All planar audio queue writer tests passed.\n";
    return 0;
}
