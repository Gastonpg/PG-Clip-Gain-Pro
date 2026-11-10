#include "pgclip/core/capture_session_control.hpp"

#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\\n';
        ++failures;
    }
}
}

int main() {
    using namespace pgclip;
    CaptureSessionControl session;

    check(session.state() == CaptureSessionState::Disarmed,
          "new session starts disarmed");
    check(!session.shouldCapture(), "disarmed session never captures");
    check(session.requestArm(), "request arm from disarmed");
    check(session.state() == CaptureSessionState::ArmRequested,
          "worker preparation is explicitly pending");
    check(!session.shouldCapture(), "do not capture before worker preparation");
    check(!session.requestStop(), "cannot stop before arm handshake");
    check(session.completeArm(true), "worker confirms prepared queue");
    check(session.shouldCapture(), "capture starts only after arm handshake");
    check(!session.requestArm(), "reject a second arm while active");
    check(session.requestStop(), "request producer stop");
    check(!session.shouldCapture(), "stop request immediately disables new capture blocks");
    check(!session.completeDrain(true), "worker cannot finalize before producer quiescence");
    check(session.acknowledgeProducerQuiesced(), "acknowledge callback quiescence");
    check(session.state() == CaptureSessionState::Draining,
          "worker drain phase follows producer quiescence");
    check(session.completeDrain(true), "worker publishes validated complete capture");
    check(session.state() == CaptureSessionState::Captured,
          "successful drain reaches captured state");

    check(session.requestArm(), "captured session can be armed again");
    check(session.completeArm(false), "worker reports failed preparation");
    check(session.state() == CaptureSessionState::Draining,
          "failed preparation enters cleanup before rejection");
    check(!session.requestArm(),
          "cannot retry while failed preparation is being cleaned");
    check(session.completeDrain(false),
          "worker confirms failed-preparation cleanup");
    check(session.state() == CaptureSessionState::Rejected,
          "failed preparation becomes terminal after cleanup");

    check(session.requestArm(), "rejected preparation can be retried");
    check(session.completeArm(true), "prepare second session");
    check(session.requestCancel(), "request cancellation while armed");
    check(!session.shouldCapture(), "cancel request disables further capture");
    check(session.acknowledgeProducerQuiesced(),
          "cancellation waits for producer quiescence");
    check(session.state() == CaptureSessionState::CancelDraining,
          "cancellation still requires worker cleanup");
    check(!session.requestArm(),
          "cannot reuse session before cancellation cleanup");
    check(session.completeDrain(false),
          "worker confirms cancellation cleanup");
    check(session.state() == CaptureSessionState::Cancelled,
          "cancelled session reaches terminal state");
    check(!session.completeDrain(true),
          "cancelled session cannot be published as captured");

    check(session.requestArm(), "cancelled and cleaned session can be armed again");
    session.reject();
    check(session.state() == CaptureSessionState::RejectRequested,
          "runtime fault requests rejection without publishing terminal state");
    check(!session.shouldCapture(), "rejected request stops accepting blocks");
    check(!session.requestArm(),
          "cannot rearm while callback quiescence is pending");
    check(session.acknowledgeProducerQuiesced(),
          "faulted producer must become quiescent");
    check(session.state() == CaptureSessionState::Draining,
          "faulted session enters worker cleanup after quiescence");
    check(!session.requestArm(),
          "cannot rearm while worker drains rejected session");
    check(session.completeDrain(false),
          "worker completes rejected-session cleanup");
    check(session.state() == CaptureSessionState::Rejected,
          "rejection is terminal only after cleanup");
    check(session.requestArm(), "cleaned rejected session can be retried");

    if (failures != 0) {
        std::cerr << failures << " capture session control test(s) failed.\\n";
        return 1;
    }
    std::cout << "All capture session control tests passed.\\n";
    return 0;
}
