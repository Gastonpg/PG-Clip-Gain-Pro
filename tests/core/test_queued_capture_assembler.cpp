#include "pgclip/core/queued_capture_assembler.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

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
    bool invalidFormatRejected = false;
    try {
        QueuedCaptureAssembler invalidFormat(
            AudioFormat{48000U, 0U}, 8U, 4U);
    } catch (const std::invalid_argument&) {
        invalidFormatRejected = true;
    }
    check(invalidFormatRejected,
          "reject zero-channel capture format without dividing by zero");

    SpscAudioBlockQueue queue(4U, 4U, 2U);
    QueuedCaptureAssembler assembler(format, 8U, 4U);

    check(assembler.begin(queue, 100U, 6U),
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

    SpscAudioBlockQueue staleQueue(4U, 4U, 2U);
    QueuedCaptureAssembler staleAssembler(format, 8U, 4U);
    check(staleQueue.tryPush(AudioBlockView{
              channelsA.data(), 50U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "prepare stale queue content");
    check(!staleAssembler.begin(staleQueue, 100U, 2U),
          "refuse to start a new capture while stale blocks remain queued");

    SpscAudioBlockQueue gapQueue(4U, 4U, 2U);
    QueuedCaptureAssembler gapAssembler(format, 8U, 4U);
    check(gapAssembler.begin(gapQueue, 200U, 6U), "begin discontinuity fixture");
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
    check(partialAssembler.begin(partialQueue, 300U, 6U), "begin partial fixture");
    check(partialQueue.tryPush(AudioBlockView{
              channelsA.data(), 300U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue incomplete capture prefix");
    check(partialAssembler.drainOne(partialQueue) ==
              QueueDrainStatus::BlockAccepted,
          "drain incomplete capture prefix");
    check(partialAssembler.finish(partialQueue),
          "finalize only after worker has drained all queued blocks");
    check(partialAssembler.descriptor().state == CaptureState::Partial,
          "finish incomplete region as partial, never complete");

    SpscAudioBlockQueue undersizedQueue(4U, 4U, 2U);
    QueuedCaptureAssembler undersizedAssembler(format, 8U, 2U);
    check(undersizedAssembler.begin(undersizedQueue, 400U, 4U),
          "begin capture with deliberately undersized worker scratch");
    const std::array<float, 4> longLeft{0.1F, 0.2F, 0.3F, 0.4F};
    const std::array<float, 4> longRight{0.5F, 0.6F, 0.7F, 0.8F};
    const std::array<const void*, 2> longChannels{
        longLeft.data(), longRight.data()
    };
    check(undersizedQueue.tryPush(AudioBlockView{
              longChannels.data(), 400U, 4U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue block larger than worker scratch");
    check(undersizedAssembler.drainOne(undersizedQueue) ==
              QueueDrainStatus::QueueError,
          "report queue destination mismatch");
    check(undersizedAssembler.descriptor().state == CaptureState::Partial,
          "do not leave capture active after worker queue error");

    SpscAudioBlockQueue wrongChannelQueue(4U, 4U, 2U);
    QueuedCaptureAssembler wrongChannelAssembler(format, 8U, 4U);
    check(wrongChannelAssembler.begin(wrongChannelQueue, 500U, 2U),
          "begin wrong-channel capture fixture");
    const std::array<const void*, 1> monoChannel{leftA.data()};
    check(wrongChannelQueue.tryPush(AudioBlockView{
              monoChannel.data(), 500U, 2U, 1U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue mismatched channel layout");
    check(wrongChannelAssembler.drainOne(wrongChannelQueue) ==
              QueueDrainStatus::CaptureRejected &&
              wrongChannelAssembler.descriptor().state == CaptureState::Cancelled,
          "cancel capture when queued channel layout does not match");

    SpscAudioBlockQueue userDefinedQueue(4U, 4U, 2U);
    QueuedCaptureAssembler userDefinedAssembler(format, 8U, 4U);
    check(userDefinedAssembler.begin(userDefinedQueue, 600U, 8U),
          "begin capture with maximum region capacity");
    check(userDefinedQueue.tryPush(AudioBlockView{
              channelsA.data(), 600U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue user-defined region content");
    check(userDefinedAssembler.drainOne(userDefinedQueue) ==
              QueueDrainStatus::BlockAccepted,
          "drain user-defined region content");
    check(!userDefinedAssembler.finishAtCapturedLength(userDefinedQueue),
          "refuse to finalize while queued audio remains");
    check(userDefinedAssembler.drainOne(userDefinedQueue) ==
              QueueDrainStatus::BlockAccepted,
          "drain pending audio before capture finalization");
    check(userDefinedAssembler.finishAtCapturedLength(userDefinedQueue),
          "finalize user-defined region after queue drains");
    check(userDefinedAssembler.descriptor().state == CaptureState::Complete &&
              userDefinedAssembler.descriptor().totalFrames == 2U,
          "accept explicit user stop as the exact captured region boundary");

    SpscAudioBlockQueue overflowQueue(2U, 4U, 2U);
    QueuedCaptureAssembler overflowAssembler(format, 8U, 4U);
    check(overflowAssembler.begin(overflowQueue, 700U, 8U),
          "begin queue overflow fixture");
    check(overflowQueue.tryPush(AudioBlockView{
              channelsA.data(), 700U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "enqueue first block before overflow");
    check(overflowQueue.tryPush(AudioBlockView{
              channelsA.data(), 702U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Success,
          "fill bounded queue before overflow");
    check(overflowQueue.tryPush(AudioBlockView{
              channelsA.data(), 704U, 2U, 2U,
              AudioSampleFormat::Float32, 0U
          }) == AudioQueueResult::Full,
          "force callback-side queue overflow");
    check(overflowAssembler.drainOne(overflowQueue) ==
              QueueDrainStatus::CaptureRejected &&
              overflowAssembler.descriptor().state == CaptureState::Discontinuous,
          "never publish capture as complete after a dropped callback block");

    if (failures != 0) {
        std::cerr << failures << " queued capture test(s) failed.\n";
        return 1;
    }

    std::cout << "All queued capture assembly tests passed.\n";
    return 0;
}
