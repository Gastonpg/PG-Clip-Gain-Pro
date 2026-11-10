#include "pgclip/core/capture_session_control.hpp"

namespace pgclip {

bool CaptureSessionControl::requestArm() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    for (;;) {
        if (current != CaptureSessionState::Disarmed &&
            current != CaptureSessionState::Captured &&
            current != CaptureSessionState::Rejected &&
            current != CaptureSessionState::Cancelled) {
            return false;
        }
        if (state_.compare_exchange_weak(
                current, CaptureSessionState::ArmRequested,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool CaptureSessionControl::completeArm(bool workerPrepared) noexcept {
    auto expected = CaptureSessionState::ArmRequested;
    return state_.compare_exchange_strong(
        expected,
        workerPrepared ? CaptureSessionState::Armed
                       : CaptureSessionState::Rejected,
        std::memory_order_acq_rel,
        std::memory_order_acquire);
}

bool CaptureSessionControl::requestStop() noexcept {
    auto expected = CaptureSessionState::Armed;
    return state_.compare_exchange_strong(
        expected, CaptureSessionState::StopRequested,
        std::memory_order_acq_rel, std::memory_order_acquire);
}

bool CaptureSessionControl::requestCancel() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    for (;;) {
        if (current != CaptureSessionState::ArmRequested &&
            current != CaptureSessionState::Armed &&
            current != CaptureSessionState::StopRequested) {
            return false;
        }
        if (state_.compare_exchange_weak(
                current, CaptureSessionState::CancelRequested,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool CaptureSessionControl::acknowledgeProducerQuiesced() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    for (;;) {
        CaptureSessionState next{};
        if (current == CaptureSessionState::StopRequested ||
            current == CaptureSessionState::RejectRequested) {
            next = CaptureSessionState::Draining;
        } else if (current == CaptureSessionState::CancelRequested) {
            next = CaptureSessionState::CancelDraining;
        } else {
            return false;
        }
        if (state_.compare_exchange_weak(
                current, next,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool CaptureSessionControl::completeDrain(
    bool captureIsCompleteAndValid) noexcept {
    auto current = state_.load(std::memory_order_acquire);
    for (;;) {
        CaptureSessionState next{};
        if (current == CaptureSessionState::Draining) {
            next = captureIsCompleteAndValid
                ? CaptureSessionState::Captured
                : CaptureSessionState::Rejected;
        } else if (current == CaptureSessionState::CancelDraining) {
            next = CaptureSessionState::Cancelled;
        } else {
            return false;
        }
        if (state_.compare_exchange_weak(
                current, next,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

void CaptureSessionControl::reject() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    for (;;) {
        if (current == CaptureSessionState::Disarmed ||
            current == CaptureSessionState::Captured ||
            current == CaptureSessionState::Rejected ||
            current == CaptureSessionState::Cancelled ||
            current == CaptureSessionState::RejectRequested ||
            current == CaptureSessionState::Draining ||
            current == CaptureSessionState::CancelDraining) {
            return;
        }
        if (state_.compare_exchange_weak(
                current, CaptureSessionState::RejectRequested,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return;
        }
    }
}

CaptureSessionState CaptureSessionControl::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

bool CaptureSessionControl::shouldCapture() const noexcept {
    return state() == CaptureSessionState::Armed;
}

} // namespace pgclip
