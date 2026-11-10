#pragma once

#include <atomic>
#include <cstdint>

static_assert(std::atomic<std::uint8_t>::is_always_lock_free,
              "Capture session state requires lock-free byte atomics.");

namespace pgclip {

enum class CaptureSessionState : std::uint8_t {
    Disarmed,
    ArmRequested,
    Armed,
    StopRequested,
    CancelRequested,
    Draining,
    Captured,
    Rejected,
    Cancelled
};

// Atomic lifecycle handshake shared by the control, audio-producer and worker
// sides. It owns no audio memory and performs no blocking operations.
class CaptureSessionControl {
public:
    CaptureSessionControl() = default;
    CaptureSessionControl(const CaptureSessionControl&) = delete;
    CaptureSessionControl& operator=(const CaptureSessionControl&) = delete;

    bool requestArm() noexcept;
    bool completeArm(bool workerPrepared) noexcept;
    bool requestStop() noexcept;
    bool requestCancel() noexcept;

    // Call only after the host guarantees the audio callback is no longer
    // executing and cannot start another producer operation.
    bool acknowledgeProducerQuiesced() noexcept;

    // Worker calls this only after draining and validating the capture.
    bool completeDrain(bool captureIsCompleteAndValid) noexcept;

    // Marks an active session unusable (e.g. queue overflow or discontinuity).
    void reject() noexcept;

    CaptureSessionState state() const noexcept;
    bool shouldCapture() const noexcept;

private:
    std::atomic<CaptureSessionState> state_{CaptureSessionState::Disarmed};
};

} // namespace pgclip
