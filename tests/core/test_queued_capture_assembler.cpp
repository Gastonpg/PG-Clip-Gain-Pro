#include "pgclip/core/queued_capture_assembler.hpp"

#include <array>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}

int main() {
    using namespace pgclip;

    const AudioFormat format{48000U, 2U};
    SpscAudioBlockQueue queue(4U, 4U, 2U);
    QueuedCaptureAssembler assembler(format, 8U, 4U);

    check(assembler.begin(100U, 6U),
          "begin worker-side capture assembly");

    std::array<float, 2> leftA{0.1F, 0.2F};
    std::array<float, 2> rightA{0.3F, 0.4F};
    const std::array<const void*, 2> channelsA{
        leftA.data(), rightA.data()
    };
    check(queue.tryPush(AudioBlockView{
              channelsA.data(), 100U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue first contiguous capture block");

    std::array<float, 4> leftB{0.5F, 0.6F, 0.7F, 0.8F};
    std::array<float, 4> rightB{-0.1F, -0.2F, -0.3F, -0.4F};
    const std::array<const void*, 2> channelsB{
        leftB.data(), rightB.data()
    };
    check(queue.tryPush(AudioBlockView{
              channelsB.data(), 102U, 4U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue final contiguous capture block");

    check(assembler.drainOne(queue) == QueueDrainStatus::BlockAccepted,
          "drain first block into bounded capture");
    check(assembler.drainOne(queue) == QueueDrainStatus::CaptureComplete,
          "complete capture after exact expected coverage");
    const auto complete = assembler.descriptor();
    check(complete.state == CaptureState::Complete &&
              complete.totalFrames == 6U &&
              !complete.hasDiscontinuities,
          "publish complete continuous capture descriptor");
    check(assembler.sampleCount() == 12U &&
              std::abs(assembler.data()[0] - 0.1F) < 1.0e-6F &&
              std::abs(assembler.data()[11] + 0.4F) < 1.0e-6F,
          "assemble interleaved samples in original frame order");
    check(assembler.drainOne(queue) == QueueDrainStatus::CaptureRejected,
          "do not consume more blocks after capture completes");

    SpscAudioBlockQueue gapQueue(4U, 4U, 2U);
    QueuedCaptureAssembler gapAssembler(format, 8U, 4U);
    check(gapAssembler.begin(200U, 6U), "begin discontinuity fixture");
    check(gapQueue.tryPush(AudioBlockView{
              channelsA.data(), 200U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue block before timeline gap");
    check(gapQueue.tryPush(AudioBlockView{
              channelsB.data(), 203U, 4U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue block after timeline gap");
    check(gapAssembler.drainOne(gapQueue) == QueueDrainStatus::BlockAccepted,
          "accept contiguous prefix before gap");
    check(gapAssembler.drainOne(gapQueue) == QueueDrainStatus::CaptureRejected,
          "reject gap during worker assembly");
    const auto discontinuous = gapAssembler.descriptor();
    check(discontinuous.state == CaptureState::Discontinuous &&
              discontinuous.hasDiscontinuities &&
              discontinuous.totalFrames == 2U,
          "mark discontinuous capture and preserve only verified prefix");

    SpscAudioBlockQueue partialQueue(4U, 4U, 2U);
    QueuedCaptureAssembler partialAssembler(format, 8U, 4U);
    check(partialAssembler.begin(300U, 6U), "begin partial fixture");
    check(partialQueue.tryPush(AudioBlockView{
              channelsA.data(), 300U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue incomplete capture prefix");
    check(partialAssembler.drainOne(partialQueue) ==
              QueueDrainStatus::BlockAccepted,
          "drain incomplete capture prefix");
    partialAssembler.finish();
    check(partialAssembler.descriptor().state == CaptureState::Partial,
          "finish incomplete region as partial, never complete");

    if (failures != 0) {
        std::cerr << failures << " queued capture test(s) failed.\n";
        return 1;
    }

    std::cout << "All queued capture assembly tests passed.\n";
    return 0;
}
