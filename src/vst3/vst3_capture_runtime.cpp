#include "vst3_capture_runtime.h"

#include "pgclip/core/planar_audio_queue_writer.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>

namespace pgclip {
namespace {

constexpr std::uint32_t kQueueSlots = 128U;
constexpr std::uint32_t kMaxFramesPerSlot = 2048U;
constexpr std::uint32_t kMaxChannels = 2U;
constexpr std::uint64_t kCaptureMemoryBudgetBytes = 16U * 1024U * 1024U;
constexpr std::uint64_t kMaximumCaptureSeconds = 60U;

class CallbackGuard {
public:
    explicit CallbackGuard(std::atomic<std::uint32_t>& count) noexcept
        : count_(count) {
        count_.fetch_add(1U, std::memory_order_acq_rel);
    }
    ~CallbackGuard() {
        count_.fetch_sub(1U, std::memory_order_release);
    }
    CallbackGuard(const CallbackGuard&) = delete;
    CallbackGuard& operator=(const CallbackGuard&) = delete;
private:
    std::atomic<std::uint32_t>& count_;
};

} // namespace

Vst3CaptureRuntime::~Vst3CaptureRuntime() {
    shutdown();
}

bool Vst3CaptureRuntime::start() noexcept {
    if (worker_.joinable()) {
        return true;
    }

    try {
        queue_ = std::make_unique<SpscAudioBlockQueue>(
            kQueueSlots, kMaxFramesPerSlot, kMaxChannels);
        discardScratch_.resize(static_cast<std::size_t>(kMaxFramesPerSlot) * kMaxChannels);
        if (!queue_->isInitialized()) {
            queue_.reset();
            return false;
        }
        shutdownRequested_.store(false, std::memory_order_release);
        worker_ = std::thread(&Vst3CaptureRuntime::workerLoop, this);
        return true;
    } catch (...) {
        queue_.reset();
        return false;
    }
}

void Vst3CaptureRuntime::shutdown() noexcept {
    if (!worker_.joinable()) {
        queue_.reset();
        return;
    }

    const auto current = session_.state();
    if (current == CaptureSessionState::Armed ||
        current == CaptureSessionState::ArmRequested ||
        current == CaptureSessionState::StopRequested) {
        session_.requestCancel();
    }
    shutdownRequested_.store(true, std::memory_order_release);
    worker_.join();
    assembler_.reset();
    queue_.reset();
}

bool Vst3CaptureRuntime::requestArm(
    std::uint32_t sampleRate,
    std::uint32_t channelCount) noexcept {
    if (!queue_ || sampleRate == 0U ||
        (channelCount != 1U && channelCount != 2U) ||
        shutdownRequested_.load(std::memory_order_acquire)) {
        return false;
    }

    const auto current = session_.state();
    if (current != CaptureSessionState::Disarmed &&
        current != CaptureSessionState::Captured &&
        current != CaptureSessionState::Rejected &&
        current != CaptureSessionState::Cancelled) {
        return false;
    }

    requestedSampleRate_.store(sampleRate, std::memory_order_relaxed);
    requestedChannels_.store(channelCount, std::memory_order_relaxed);
    droppedBlocksAtArm_.store(queue_->droppedBlocks(), std::memory_order_relaxed);
    firstBlockReady_.store(false, std::memory_order_relaxed);
    continuity_.reset();
    lastCapturedFrames_.store(0U, std::memory_order_relaxed);
    lastCaptureValid_.store(false, std::memory_order_relaxed);
    return session_.requestArm();
}

void Vst3CaptureRuntime::requestStop() noexcept {
    if (!session_.requestStop() &&
        session_.state() == CaptureSessionState::ArmRequested) {
        session_.requestCancel();
    }
}

void Vst3CaptureRuntime::captureBlock(
    const void* const* channelData,
    std::uint32_t channelCount,
    std::uint32_t frameCount,
    std::int64_t projectTimeSamples,
    bool playing,
    bool cycleActive,
    AudioSampleFormat sampleFormat,
    std::uint64_t silenceFlags) noexcept {
    CallbackGuard guard(callbacksInFlight_);

    if (session_.state() != CaptureSessionState::Armed) {
        return;
    }

    const auto continuity = continuity_.inspect(
        projectTimeSamples, frameCount, playing, cycleActive);

    if (continuity.status == TransportContinuityStatus::NotPlaying) {
        session_.requestStop();
        return;
    }
    if (continuity.status != TransportContinuityStatus::FirstBlock &&
        continuity.status != TransportContinuityStatus::Contiguous) {
        session_.reject();
        return;
    }
    if (projectTimeSamples < 0) {
        session_.reject();
        return;
    }

    if (continuity.status == TransportContinuityStatus::FirstBlock) {
        firstProjectSample_.store(projectTimeSamples, std::memory_order_relaxed);
        firstBlockReady_.store(true, std::memory_order_release);
    }

    const auto report = pushPlanarAudioInChunks(
        *queue_, channelData, channelCount, frameCount, kMaxFramesPerSlot,
        static_cast<std::uint64_t>(projectTimeSamples), sampleFormat,
        silenceFlags);

    if (!report.complete) {
        session_.reject();
    }
}

void Vst3CaptureRuntime::workerLoop() noexcept {
    for (;;) {
        const auto current = session_.state();

        if (current == CaptureSessionState::ArmRequested) {
            assembler_.reset();
            assemblerStarted_ = false;

            const auto sampleRate =
                requestedSampleRate_.load(std::memory_order_acquire);
            const auto channels =
                requestedChannels_.load(std::memory_order_acquire);
            const auto bytesPerFrame =
                static_cast<std::uint64_t>(channels) * sizeof(float);
            const auto memoryLimitedFrames =
                bytesPerFrame == 0U ? 0U : kCaptureMemoryBudgetBytes / bytesPerFrame;
            const auto timeLimitedFrames =
                static_cast<std::uint64_t>(sampleRate) * kMaximumCaptureSeconds;
            const auto capacityFrames =
                std::min(memoryLimitedFrames, timeLimitedFrames);

            bool prepared = sampleRate > 0U &&
                            (channels == 1U || channels == 2U) &&
                            capacityFrames > 0U;
            if (prepared) {
                try {
                    assembler_ = std::make_unique<QueuedCaptureAssembler>(
                        AudioFormat{sampleRate, channels}, capacityFrames,
                        kMaxFramesPerSlot);
                } catch (...) {
                    prepared = false;
                    assembler_.reset();
                }
            }

            if (session_.completeArm(prepared) && !prepared) {
                session_.completeDrain(false);
            }
        } else if (current == CaptureSessionState::Armed) {
            if (assembler_ && !assemblerStarted_ &&
                firstBlockReady_.load(std::memory_order_acquire)) {
                const auto firstSample =
                    firstProjectSample_.load(std::memory_order_relaxed);
                const auto capacityFrames = std::min(
                    kCaptureMemoryBudgetBytes /
                        (static_cast<std::uint64_t>(
                            requestedChannels_.load(std::memory_order_relaxed)) *
                         sizeof(float)),
                    static_cast<std::uint64_t>(
                        requestedSampleRate_.load(std::memory_order_relaxed)) *
                        kMaximumCaptureSeconds);

                assemblerStarted_ = assembler_->beginFromActiveProducer(
                    *queue_, static_cast<std::uint64_t>(firstSample),
                    capacityFrames,
                    droppedBlocksAtArm_.load(std::memory_order_relaxed));
                if (!assemblerStarted_) {
                    session_.reject();
                }
            }

            if (assemblerStarted_ && assembler_) {
                while (queue_->queuedBlocks() > 0U) {
                    const auto drain = assembler_->drainOne(*queue_);
                    if (drain == QueueDrainStatus::CaptureRejected ||
                        drain == QueueDrainStatus::QueueError) {
                        session_.reject();
                        break;
                    }
                    if (drain == QueueDrainStatus::CaptureComplete) {
                        session_.requestStop();
                        break;
                    }
                    if (drain == QueueDrainStatus::QueueEmpty) {
                        break;
                    }
                }
            }
        } else if (current == CaptureSessionState::StopRequested) {
            if (callbacksInFlight_.load(std::memory_order_acquire) == 0U) {
                if (session_.acknowledgeProducerQuiesced()) {
                    bool valid = false;
                    if (assembler_ && !assemblerStarted_ &&
                        firstBlockReady_.load(std::memory_order_acquire)) {
                        const auto firstSample =
                            firstProjectSample_.load(std::memory_order_relaxed);
                        const auto channels =
                            requestedChannels_.load(std::memory_order_relaxed);
                        const auto capacityFrames = std::min(
                            kCaptureMemoryBudgetBytes /
                                (static_cast<std::uint64_t>(channels) * sizeof(float)),
                            static_cast<std::uint64_t>(
                                requestedSampleRate_.load(std::memory_order_relaxed)) *
                                kMaximumCaptureSeconds);
                        assemblerStarted_ = assembler_->beginFromActiveProducer(
                            *queue_, static_cast<std::uint64_t>(firstSample),
                            capacityFrames,
                            droppedBlocksAtArm_.load(std::memory_order_relaxed));
                    }
                    if (assemblerStarted_ && assembler_) {
                        bool drainOk = true;
                        while (queue_->queuedBlocks() > 0U) {
                            const auto drain = assembler_->drainOne(*queue_);
                            if (drain == QueueDrainStatus::CaptureRejected ||
                                drain == QueueDrainStatus::QueueError) {
                                drainOk = false;
                                break;
                            }
                            if (drain == QueueDrainStatus::QueueEmpty) {
                                break;
                            }
                        }
                        const bool finished =
                            assembler_->finishAtCapturedLength(*queue_);
                        const auto descriptor = assembler_->descriptor();
                        valid = drainOk && queue_->queuedBlocks() == 0U &&
                                queue_->droppedBlocks() ==
                                    droppedBlocksAtArm_.load(std::memory_order_relaxed) &&
                                finished &&
                                descriptor.state == CaptureState::Complete &&
                                descriptor.totalFrames > 0U;
                        lastCapturedFrames_.store(
                            descriptor.totalFrames, std::memory_order_release);
                    }
                    lastCaptureValid_.store(valid, std::memory_order_release);
                    session_.completeDrain(valid);
                }
            }
        } else if (current == CaptureSessionState::RejectRequested ||
                   current == CaptureSessionState::CancelRequested) {
            if (callbacksInFlight_.load(std::memory_order_acquire) == 0U) {
                finishRejectedSession();
            }
        } else if (current == CaptureSessionState::Draining) {
            // A failed worker preparation has no active producer.
            session_.completeDrain(false);
        }

        if (shutdownRequested_.load(std::memory_order_acquire)) {
            const auto state = session_.state();
            if (callbacksInFlight_.load(std::memory_order_acquire) == 0U &&
                (state == CaptureSessionState::Disarmed ||
                 state == CaptureSessionState::Captured ||
                 state == CaptureSessionState::Rejected ||
                 state == CaptureSessionState::Cancelled)) {
                break;
            }
            if (state == CaptureSessionState::ArmRequested) {
                session_.requestCancel();
            } else if (state == CaptureSessionState::Armed) {
                session_.requestCancel();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Vst3CaptureRuntime::finishRejectedSession() noexcept {
    if (!session_.acknowledgeProducerQuiesced()) {
        return;
    }
    if (assembler_) {
        assembler_->cancel();
    }
    if (queue_) {
        QueuedAudioBlockInfo info{};
        while (queue_->tryPop(
                   discardScratch_.data(), discardScratch_.size(), info) ==
               AudioQueueResult::Success) {
        }
    }
    lastCaptureValid_.store(false, std::memory_order_release);
    lastCapturedFrames_.store(0U, std::memory_order_release);
    session_.completeDrain(false);
    assembler_.reset();
    assemblerStarted_ = false;
}

CaptureSessionState Vst3CaptureRuntime::state() const noexcept {
    return session_.state();
}

std::uint64_t Vst3CaptureRuntime::lastCapturedFrames() const noexcept {
    return lastCapturedFrames_.load(std::memory_order_acquire);
}

bool Vst3CaptureRuntime::lastCaptureValid() const noexcept {
    return lastCaptureValid_.load(std::memory_order_acquire);
}

} // namespace pgclip
