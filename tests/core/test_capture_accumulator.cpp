#include "pgclip/core/capture_accumulator.hpp"

#include <iostream>
#include <limits>

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

    CaptureAccumulator capture(AudioFormat{48000U, 2U}, 8U);
    check(capture.begin(1000U, 4U), "begin capture with expected length");

    const float firstBlock[] = {0.1F, -0.1F, 0.2F, -0.2F};
    const float secondBlock[] = {0.3F, -0.3F, 0.4F, -0.4F};

    check(capture.append(1000U, firstBlock, 2U) ==
              CaptureAppendResult::Accepted,
          "accept first contiguous block");
    check(capture.append(1002U, secondBlock, 2U) ==
              CaptureAppendResult::Accepted,
          "accept second contiguous block");
    check(capture.capturedFrames() == 4U, "count frames, not samples");
    check(capture.sampleCount() == 8U, "count interleaved samples");
    check(capture.data()[4U] == 0.3F && capture.data()[7U] == -0.4F,
          "copy blocks into preallocated storage in order");
    capture.finish();
    check(capture.descriptor().state == CaptureState::Complete,
          "finish contiguous capture as complete");
    check(!validate(capture.descriptor()), "complete capture descriptor validates");
    check(capture.append(1004U, firstBlock, 1U) ==
              CaptureAppendResult::NotCapturing,
          "reject blocks after finish");

    CaptureAccumulator gap(AudioFormat{48000U, 1U}, 16U);
    check(gap.begin(0U, 8U), "begin gap capture");
    const float monoBlock[] = {0.1F, 0.2F};
    check(gap.append(4U, monoBlock, 2U) == CaptureAppendResult::GapDetected,
          "detect missing initial frames");
    check(gap.descriptor().state == CaptureState::Discontinuous &&
              gap.descriptor().hasDiscontinuities,
          "mark gap as discontinuous");

    CaptureAccumulator overlap(AudioFormat{48000U, 1U}, 16U);
    check(overlap.begin(20U, 8U), "begin overlap capture");
    check(overlap.append(20U, monoBlock, 2U) == CaptureAppendResult::Accepted,
          "accept initial block before overlap test");
    check(overlap.append(21U, monoBlock, 2U) ==
              CaptureAppendResult::OverlapDetected,
          "detect overlapping or repeated block");

    CaptureAccumulator bounded(AudioFormat{48000U, 1U}, 2U);
    check(bounded.begin(0U, 2U), "begin bounded capture");
    check(bounded.append(0U, monoBlock, 2U) == CaptureAppendResult::Accepted,
          "fill bounded capture exactly");
    check(bounded.append(2U, monoBlock, 1U) ==
              CaptureAppendResult::CapacityExceeded,
          "reject block beyond fixed capacity");
    check(bounded.descriptor().state == CaptureState::Partial,
          "capacity overflow leaves partial capture");

    CaptureAccumulator truncated(AudioFormat{48000U, 1U}, 8U);
    check(truncated.begin(0U, 8U), "begin capture that may be truncated");
    check(truncated.append(0U, monoBlock, 2U) == CaptureAppendResult::Accepted,
          "accept partial block before early stop");
    truncated.finish();
    check(truncated.descriptor().state == CaptureState::Partial,
          "do not mark early-stopped region complete");

    CaptureAccumulator tooLargeBegin(AudioFormat{48000U, 1U}, 4U);
    check(!tooLargeBegin.begin(0U, 5U),
          "reject expected capture longer than preallocated capacity");
    check(tooLargeBegin.descriptor().state == CaptureState::Partial,
          "oversized capture request remains explicitly partial");

    CaptureAccumulator invalidBegin(AudioFormat{48000U, 1U}, 4U);
    check(!invalidBegin.begin(0U, 0U),
          "reject zero-length expected capture region");
    check(invalidBegin.descriptor().state == CaptureState::Partial &&
              !invalidBegin.descriptor().hasDiscontinuities,
          "invalid capture request is not mislabeled as a timeline gap");

    CaptureAccumulator overflowBegin(AudioFormat{48000U, 1U}, 4U);
    check(!overflowBegin.begin(
              std::numeric_limits<FrameIndex>::max() - 1U, 4U),
          "reject expected region that overflows frame index");
    check(!overflowBegin.descriptor().hasDiscontinuities,
          "timeline overflow is not reported as an audio gap");

    CaptureAccumulator cancelled(AudioFormat{48000U, 1U}, 4U);
    check(cancelled.begin(0U, 4U), "begin cancellable capture");
    cancelled.cancel();
    check(cancelled.descriptor().state == CaptureState::Cancelled,
          "record cancellation state");

    if (failures != 0) {
        std::cerr << failures << " capture test(s) failed.\n";
        return 1;
    }

    std::cout << "All capture accumulator tests passed.\n";
    return 0;
}
