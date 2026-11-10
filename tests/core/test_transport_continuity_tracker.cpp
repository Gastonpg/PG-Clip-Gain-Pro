#include "pgclip/core/transport_continuity_tracker.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

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
    TransportContinuityTracker tracker;

    auto report = tracker.inspect(1000, 128, true, false);
    check(report.status == TransportContinuityStatus::FirstBlock &&
              report.expectedNextSample == 1128,
          "first playing block establishes expected position");

    report = tracker.inspect(1128, 64, true, false);
    check(report.status == TransportContinuityStatus::Contiguous &&
              report.expectedNextSample == 1192,
          "contiguous block advances expected position");

    report = tracker.inspect(1260, 64, true, false);
    check(report.status == TransportContinuityStatus::Discontinuity &&
              !report.hasExpectedNextSample,
          "forward seek or gap invalidates continuity");

    report = tracker.inspect(900, 64, true, false);
    check(report.status == TransportContinuityStatus::FirstBlock,
          "new capture can establish a fresh origin after reset");

    report = tracker.inspect(950, 32, true, false);
    check(report.status == TransportContinuityStatus::Discontinuity,
          "overlap or backward movement is rejected");

    report = tracker.inspect(2000, 64, false, false);
    check(report.status == TransportContinuityStatus::NotPlaying,
          "stopped transport resets continuity");

    report = tracker.inspect(3000, 64, true, true);
    check(report.status == TransportContinuityStatus::CycleActive,
          "cycle playback is rejected explicitly");

    report = tracker.inspect(4000, 0, true, false);
    check(report.status == TransportContinuityStatus::InvalidBlock,
          "zero-frame blocks are invalid");

    report = tracker.inspect(
        std::numeric_limits<std::int64_t>::max() - 10, 32, true, false);
    check(report.status == TransportContinuityStatus::InvalidBlock,
          "sample-position overflow is rejected");

    report = tracker.inspect(-128, 128, true, false);
    check(report.status == TransportContinuityStatus::FirstBlock &&
              report.expectedNextSample == 0,
          "negative preroll sample positions remain valid");

    if (failures != 0) {
        std::cerr << failures << " transport continuity test(s) failed.\n";
        return 1;
    }
    std::cout << "All transport continuity tests passed.\n";
    return 0;
}
