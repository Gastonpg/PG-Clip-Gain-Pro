#include "pgclip/core/transport_continuity_tracker.hpp"

#include <limits>

namespace pgclip {

TransportContinuityReport TransportContinuityTracker::inspect(
    std::int64_t projectTimeSamples,
    std::uint32_t frameCount,
    bool playing,
    bool cycleActive) noexcept {
    if (frameCount == 0U) {
        reset();
        return {TransportContinuityStatus::InvalidBlock, 0, false};
    }
    if (!playing) {
        reset();
        return {TransportContinuityStatus::NotPlaying, 0, false};
    }
    if (cycleActive) {
        reset();
        return {TransportContinuityStatus::CycleActive, 0, false};
    }

    const auto maxSample = std::numeric_limits<std::int64_t>::max();
    if (projectTimeSamples > maxSample -
            static_cast<std::int64_t>(frameCount)) {
        reset();
        return {TransportContinuityStatus::InvalidBlock, 0, false};
    }

    const auto nextSample =
        projectTimeSamples + static_cast<std::int64_t>(frameCount);

    if (!hasExpectedNextSample_) {
        expectedNextSample_ = nextSample;
        hasExpectedNextSample_ = true;
        return {TransportContinuityStatus::FirstBlock,
                expectedNextSample_, true};
    }

    if (projectTimeSamples != expectedNextSample_) {
        reset();
        return {TransportContinuityStatus::Discontinuity, 0, false};
    }

    expectedNextSample_ = nextSample;
    return {TransportContinuityStatus::Contiguous,
            expectedNextSample_, true};
}

void TransportContinuityTracker::reset() noexcept {
    expectedNextSample_ = 0;
    hasExpectedNextSample_ = false;
}

} // namespace pgclip
