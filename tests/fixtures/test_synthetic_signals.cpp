#include "pgclip/core/synthetic_signals.hpp"

#include <cmath>
#include <iostream>

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
    using namespace pgclip::test;

    const AudioFormat mono{48000U, 1U};
    const auto silence = makeSilence(mono, 64U);
    check(silence.has_value(), "generate silence");
    check(silence && silence->interleaved.size() == 64U,
          "silence has exact frame count");
    check(silence && std::all_of(silence->interleaved.begin(),
                                 silence->interleaved.end(),
                                 [](float value) { return value == 0.0F; }),
          "silence contains exact zeros");

    const auto sine = makeSine(mono, 480U, 440.0, 0.5);
    check(sine.has_value(), "generate sine");
    check(sine && sine->interleaved.size() == 480U,
          "sine has exact frame count");
    check(sine && sine->interleaved.front() == 0.0F,
          "sine starts at zero phase");
    check(sine && std::abs(sine->interleaved[25U]) > 0.1F,
          "sine contains expected non-zero samples");

    const auto stereo = makeSine(AudioFormat{48000U, 2U}, 32U,
                                 1000.0, 0.25, true);
    check(stereo && stereo->interleaved.size() == 64U,
          "stereo fixture is interleaved");
    check(stereo && stereo->interleaved[1U] == -stereo->interleaved[0U],
          "right channel polarity can be inverted");

    const auto impulse = makeImpulse(AudioFormat{48000U, 2U}, 16U, 7U, 0.75F);
    check(impulse.has_value(), "generate stereo impulse");
    check(impulse && impulse->interleaved[14U] == 0.75F &&
                       impulse->interleaved[15U] == 0.75F,
          "impulse appears at exact frame on both channels");
    check(impulse && impulse->interleaved[12U] == 0.0F,
          "impulse does not affect adjacent frames");

    check(!makeSine(mono, 16U, 24000.0, 0.5),
          "reject sine at Nyquist");
    check(!makeSine(mono, 16U, 440.0, 1.5),
          "reject amplitude above full scale");
    check(!makeImpulse(mono, 16U, 16U, 0.5F),
          "reject impulse outside frame range");
    check(!makeSilence(AudioFormat{48000U, 4U}, 16U),
          "reject unsupported channel layout");

    if (failures != 0) {
        std::cerr << failures << " fixture test(s) failed.\n";
        return 1;
    }

    std::cout << "All synthetic fixture tests passed.\n";
    return 0;
}
