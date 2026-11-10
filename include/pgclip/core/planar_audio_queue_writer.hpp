#pragma once

#include "pgclip/core/spsc_audio_block_queue.hpp"

#include <cstdint>

namespace pgclip {

struct AudioQueueWriteReport {
    AudioQueueResult result{AudioQueueResult::Success};
    std::uint32_t acceptedBlocks{0};
    std::uint32_t acceptedFrames{0};
    bool complete{false};
};

// Realtime-safe adapter for planar host buffers. Splits oversized host blocks
// into preallocated queue slots; it performs no allocation or waiting.
AudioQueueWriteReport pushPlanarAudioInChunks(
    SpscAudioBlockQueue& queue,
    const void* const* channelData,
    std::uint32_t channelCount,
    std::uint32_t frameCount,
    std::uint32_t maxFramesPerChunk,
    std::uint64_t sourceStartFrame,
    AudioSampleFormat sampleFormat,
    std::uint64_t silenceFlags) noexcept;

} // namespace pgclip
