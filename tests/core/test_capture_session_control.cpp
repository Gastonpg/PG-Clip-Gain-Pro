#include "pgclip/core/capture_session_control.hpp"

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
    check(!session.completeArm(false), "failed preparation cannot arm session");
    check(session.state() == CaptureSessionState::Rejected,
          "failed preparation rejects session");

    check(session.requestArm(), "rejected session can be retried");
    check(session.completeArm(true), "prepare second session");
    check(session.requestCancel(), "request cancellation while armed");
    check(!session.shouldCapture(), "cancel request disables further capture");
    check(session.acknowledgeProducerQuiesced(),
          "cancellation waits for producer quiescence");
    check(session.state() == CaptureSessionState::Cancelled,
          "cancelled session reaches terminal state");
    check(!session.completeDrain(true),
          "cancelled session cannot be published as captured");

    check(session.requestArm(), "cancelled session can be armed again");
    session.reject();
    check(session.state() == CaptureSessionState::Rejected,
          "runtime discontinuity rejects active capture");
    check(!session.shouldCapture(), "rejected session stops accepting blocks");

    if (failures != 0) {
        std::cerr << failures << " capture session control test(s) failed.\n";
        return 1;
    }
    std::cout << "All capture session control tests passed.\n";
    return 0;
}
