#include "pgclip/core/window_analysis.hpp"
#include "pgclip/core/synthetic_signals.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

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

    const auto sine = makeSine(AudioFormat{48000U, 1U}, 480U, 750.0, 0.5);
    check(sine.has_value(), "create source sine");
    if (sine) {
        const AudioBufferView view{
            sine->format, sine->frameCount, sine->interleaved.data()
        };
        const auto result = analyzeWindowDescriptors(
            view, AnalysisConfig{256U, 128U, 1U});

        check(result.status == WindowAnalysisStatus::Success,
              "analyze valid audio view");
        check(result.windows.size() == 4U,
              "produce overlapping windows including final partial window");
        if (result.windows.size() == 4U) {
            check(result.windows[0].range.start == 0U &&
                      result.windows[0].range.end == 256U,
                  "first window uses half-open frame range");
            check(result.windows[3].range.start == 384U &&
                      result.windows[3].range.end == 480U,
                  "last window preserves trailing frames");
            check(std::abs(result.windows[0].channels[0].rms - 0.353553) < 0.002,
                  "calculate RMS over a four-cycle sine window");
            check(std::abs(result.windows[0].channels[0].peak - 0.5) < 1.0e-6,
                  "calculate sample peak");
            check(std::abs(result.windows[0].channels[0].meanAbsolute -
                           0.31831) < 0.002,
                  "calculate mean absolute amplitude");
            check(std::abs(result.windows[0].channels[0].dcOffset) < 1.0e-6,
                  "calculate DC offset");
            check(result.windows[0].channels[0].zeroCrossingRate > 0.02 &&
                      result.windows[0].channels[0].zeroCrossingRate < 0.04,
                  "calculate normalized zero-crossing rate");
            check(std::abs(result.windows[0].channels[0].crestFactor -
                           1.414213) < 0.002,
                  "calculate crest factor");
        }
    }

    const auto silence = makeSilence(AudioFormat{48000U, 1U}, 300U);
    if (silence) {
        const auto result = analyzeWindowDescriptors(
            AudioBufferView{silence->format, silence->frameCount,
                            silence->interleaved.data()},
            AnalysisConfig{128U, 128U, 1U});
        check(result.windows.size() == 3U, "cover all silence frames");
        check(result.windows[0].channels[0].silent &&
                  result.windows[0].channels[0].rms == 0.0 &&
                  result.windows[0].channels[0].meanAbsolute == 0.0 &&
                  result.windows[0].channels[0].zeroCrossingRate == 0.0,
              "classify digital silence without a fake loudness floor");
    }

    const auto stereo = makeSine(AudioFormat{48000U, 2U}, 256U,
                                 750.0, 0.25, true);
    if (stereo) {
        const auto result = analyzeWindowDescriptors(
            AudioBufferView{stereo->format, stereo->frameCount,
                            stereo->interleaved.data()},
            AnalysisConfig{256U, 256U, 1U});
        check(result.windows.size() == 1U &&
                  result.windows[0].channels.size() == 2U,
              "keep stereo channel metrics separate");
        if (!result.windows.empty()) {
            check(std::abs(result.windows[0].channels[0].rms -
                           result.windows[0].channels[1].rms) < 1.0e-9,
                  "polarity inversion does not cancel channel analysis");
        }
    }

    std::vector<float> invalidSamples(64U, 0.25F);
    invalidSamples[10U] = std::numeric_limits<float>::quiet_NaN();
    const auto invalid = analyzeWindowDescriptors(
        AudioBufferView{AudioFormat{48000U, 1U}, 64U,
                        invalidSamples.data()},
        AnalysisConfig{64U, 64U, 1U});
    check(invalid.windows.size() == 1U &&
              !invalid.windows[0].channels[0].finite &&
              invalid.windows[0].channels[0].meanAbsolute == 0.0 &&
              invalid.windows[0].channels[0].crestFactor == 0.0,
          "mark windows containing non-finite samples invalid without partial metrics");

    const auto badConfig = analyzeWindowDescriptors(
        AudioBufferView{AudioFormat{48000U, 1U}, 64U,
                        invalidSamples.data()},
        AnalysisConfig{64U, 0U, 1U});
    check(badConfig.status == WindowAnalysisStatus::InvalidConfiguration,
          "reject invalid analysis configuration");

    const auto badAudio = analyzeWindowDescriptors(
        AudioBufferView{AudioFormat{48000U, 1U}, 0U, nullptr},
        AnalysisConfig{64U, 32U, 1U});
    check(badAudio.status == WindowAnalysisStatus::InvalidAudio,
          "reject empty audio view");

    if (failures != 0) {
        std::cerr << failures << " window analysis test(s) failed.\n";
        return 1;
    }

    std::cout << "All window analysis tests passed.\n";
    return 0;
}
