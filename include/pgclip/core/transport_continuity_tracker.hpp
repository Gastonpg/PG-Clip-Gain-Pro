#pragma once

#include <cstdint>

namespace pgclip {

enum class TransportContinuityStatus {
    FirstBlock,
    Contiguous,
    NotPlaying,
    CycleActive,
    Discontinuity,
    InvalidBlock
};

struct TransportContinuityReport {
    TransportContinuityStatus status{TransportContinuityStatus::InvalidBlock};
    std::int64_t expectedNextSample{0};
    bool hasExpectedNextSample{false};
};

// Host-independent guard for projectTimeSamples continuity. Call once per
// audio block, in callback order. It performs no allocation or synchronization.
class TransportContinuityTracker {
public:
    TransportContinuityReport inspect(
        std::int64_t projectTimeSamples,
        std::uint32_t frameCount,
        bool playing,
        bool cycleActive) noexcept;

    void reset() noexcept;

private:
    std::int64_t expectedNextSample_{0};
    bool hasExpectedNextSample_{false};
};

} // namespace pgclip
