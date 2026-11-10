#include "pgclip/core/types.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace pgclip;

    assert(!validate(AudioFormat{48000U, 1U}));
    assert(!validate(AudioFormat{44100U, 2U}));
    assert(validate(AudioFormat{0U, 1U}).code ==
           ValidationCode::InvalidSampleRate);
    assert(validate(AudioFormat{48000U, 3U}).code ==
           ValidationCode::UnsupportedChannelCount);

    assert(!validate(FrameRange{10U, 20U}, 20U));
    assert(validate(FrameRange{20U, 20U}, 20U).code ==
           ValidationCode::EmptyRange);
    assert(validate(FrameRange{10U, 21U}, 20U).code ==
           ValidationCode::RangeOutsideCapture);

    assert(!validate(CaptureDescriptor{
        AudioFormat{48000U, 2U}, 48000U, CaptureState::Complete, false
    }));
    assert(validate(CaptureDescriptor{
        AudioFormat{48000U, 2U}, 48000U, CaptureState::Complete, true
    }));

    assert(!validate(AnalysisConfig{1024U, 256U, 1U}));
    assert(validate(AnalysisConfig{0U, 256U, 1U}).code ==
           ValidationCode::InvalidWindow);
    assert(validate(AnalysisConfig{1024U, 2048U, 1U}).code ==
           ValidationCode::InvalidHop);

    assert(!validate(GainCurve{{{0U, 0.0}, {100U, -2.5}, {200U, 1.0}}}));
    assert(validate(GainCurve{{{0U, 0.0}, {100U, 1.0}, {100U, 2.0}}}).code ==
           ValidationCode::DuplicateCurvePoint);
    assert(validate(GainCurve{{{0U, 0.0}, {100U, 1.0}, {50U, 2.0}}}).code ==
           ValidationCode::UnorderedCurvePoints);
    assert(validate(GainCurve{{{0U, std::numeric_limits<double>::infinity()}}}).code ==
           ValidationCode::NonFiniteGain);

    assert(std::abs(gainDbToLinear(0.0) - 1.0) < 1.0e-12);
    assert(std::abs(gainDbToLinear(6.020599913) - 2.0) < 1.0e-8);
    assert(std::abs(gainDbToLinear(-6.020599913) - 0.5) < 1.0e-8);
    assert(std::isnan(gainDbToLinear(std::numeric_limits<double>::quiet_NaN())));

    return 0;
}
