#include "pgclip/core/types.hpp"

#include <cmath>
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

    check(!validate(AudioFormat{48000U, 1U}), "valid mono format");
    check(!validate(AudioFormat{44100U, 2U}), "valid stereo format");
    check(validate(AudioFormat{0U, 1U}).code == ValidationCode::InvalidSampleRate,
          "reject invalid sample rate");
    check(validate(AudioFormat{48000U, 3U}).code == ValidationCode::UnsupportedChannelCount,
          "reject unsupported channel count");

    check(!validate(FrameRange{10U, 20U}, 20U), "accept valid half-open range");
    check(validate(FrameRange{20U, 20U}, 20U).code == ValidationCode::EmptyRange,
          "reject empty range");
    check(validate(FrameRange{10U, 21U}, 20U).code == ValidationCode::RangeOutsideCapture,
          "reject range beyond capture");

    check(!validate(CaptureDescriptor{
        AudioFormat{48000U, 2U}, 48000U, CaptureState::Complete, false
    }), "accept complete continuous capture");
    check(static_cast<bool>(validate(CaptureDescriptor{
        AudioFormat{48000U, 2U}, 48000U, CaptureState::Complete, true
    })), "reject complete capture with discontinuities");
    check(static_cast<bool>(validate(CaptureDescriptor{
        AudioFormat{48000U, 2U}, 48000U, CaptureState::Discontinuous, false
    })), "reject discontinuous state without discontinuity flag");

    check(!validate(AnalysisConfig{1024U, 256U, 1U}), "accept valid analysis config");
    check(validate(AnalysisConfig{0U, 256U, 1U}).code == ValidationCode::InvalidWindow,
          "reject zero analysis window");
    check(validate(AnalysisConfig{1024U, 2048U, 1U}).code == ValidationCode::InvalidHop,
          "reject hop larger than window");
    check(validate(AnalysisConfig{1024U, 256U, 0U}).code == ValidationCode::InvalidVersion,
          "reject zero config version");

    check(!validate(GainCurve{{{0U, 0.0}, {100U, -2.5}, {200U, 1.0}}}),
          "accept ordered finite gain curve");
    check(validate(GainCurve{{{0U, 0.0}, {100U, 1.0}, {100U, 2.0}}}).code ==
              ValidationCode::DuplicateCurvePoint,
          "reject duplicate curve frame");
    check(validate(GainCurve{{{0U, 0.0}, {100U, 1.0}, {50U, 2.0}}}).code ==
              ValidationCode::UnorderedCurvePoints,
          "reject unordered curve points");
    check(validate(GainCurve{{{0U, std::numeric_limits<double>::infinity()}}}).code ==
              ValidationCode::NonFiniteGain,
          "reject infinite gain");

    check(std::abs(gainDbToLinear(0.0) - 1.0) < 1.0e-12, "0 dB converts to unity");
    check(std::abs(gainDbToLinear(6.020599913) - 2.0) < 1.0e-8,
          "+6.0206 dB converts to approximately 2x");
    check(std::abs(gainDbToLinear(-6.020599913) - 0.5) < 1.0e-8,
          "-6.0206 dB converts to approximately 0.5x");
    check(std::isnan(gainDbToLinear(std::numeric_limits<double>::quiet_NaN())),
          "non-finite gain conversion remains detectable");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All core contract tests passed.\n";
    return 0;
}
