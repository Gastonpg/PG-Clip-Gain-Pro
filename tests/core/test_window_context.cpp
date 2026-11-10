#include "pgclip/core/window_context.hpp"
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

    const AudioFormat mono{48000U, 1U};
    const AnalysisConfig config{128U, 128U, 1U};
    const auto sine = makeSine(mono, 512U, 750.0, 0.5);
    check(sine.has_value(), "create context source sine");
    if (sine) {
        const auto analysis = analyzeWindowDescriptors(
            AudioBufferView{mono, sine->frameCount,
                            sine->interleaved.data()},
            config);
        const auto built = buildSharedAnalysisSnapshot(
            CaptureDescriptor{mono, 512U, CaptureState::Complete, false},
            config, analysis);
        if (built.snapshot) {
            const auto context = summarizeWindowContext(*built.snapshot);
            check(context.status == ContextAnalysisStatus::Ready,
                  "summarize non-silent window context");
            check(context.channels.size() == 1U,
                  "produce one context per channel");
            if (!context.channels.empty()) {
                const auto& channel = context.channels[0];
                check(channel.totalWindows == 4U &&
                          channel.nonSilentWindows == 4U &&
                          channel.finiteWindows + channel.invalidWindows == channel.totalWindows,
                      "count all finite non-silent windows and invalid evidence");
                check(channel.medianRms.has_value() &&
                          std::abs(*channel.medianRms - 0.353553) < 0.002,
                      "calculate median RMS descriptively");
                check(channel.medianAbsoluteDeviationRms.has_value(),
                      "calculate robust RMS dispersion");
                check(channel.minimumRms && channel.maximumRms &&
                          *channel.minimumRms <= *channel.maximumRms,
                      "retain RMS range");
            }
        } else {
            check(false, "build source snapshot for context test");
        }
    }

    const auto silence = makeSilence(mono, 256U);
    check(silence.has_value(), "create context silence fixture");
    if (silence) {
        const auto analysis = analyzeWindowDescriptors(
            AudioBufferView{mono, silence->frameCount,
                            silence->interleaved.data()},
            AnalysisConfig{128U, 128U, 1U});
        const auto built = buildSharedAnalysisSnapshot(
            CaptureDescriptor{mono, 256U, CaptureState::Complete, false},
            AnalysisConfig{128U, 128U, 1U}, analysis);
        if (built.snapshot) {
            const auto context = summarizeWindowContext(*built.snapshot);
            check(context.status == ContextAnalysisStatus::NoNonSilentWindows,
                  "silence does not invent a loudness profile");
            check(context.channels[0].silentWindows == 2U &&
                      !context.channels[0].medianRms,
                  "count silence and leave non-silent statistics absent");
        }
    }

    std::vector<float> mixed(512U, 0.2F);
    mixed[10U] = std::numeric_limits<float>::quiet_NaN();
    const auto mixedAnalysis = analyzeWindowDescriptors(
        AudioBufferView{mono, 512U, mixed.data()}, config);
    const auto mixedSnapshot = buildSharedAnalysisSnapshot(
        CaptureDescriptor{mono, 512U, CaptureState::Complete, false},
        config, mixedAnalysis);
    if (mixedSnapshot.snapshot) {
        const auto context = summarizeWindowContext(*mixedSnapshot.snapshot);
        check(context.channels[0].invalidWindows == 1U &&
                  context.channels[0].nonSilentWindows == 3U,
              "separate invalid evidence from valid windows");
    } else {
        check(false, "build mixed-validity snapshot");
    }

    if (failures != 0) {
        std::cerr << failures << " context test(s) failed.\n";
        return 1;
    }

    std::cout << "All window context tests passed.\n";
    return 0;
}
