#include "pgclip/core/clip_peak_foundation.hpp"
#include "pgclip/core/window_analysis.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void addSine(std::vector<float>& samples,
             std::size_t start,
             std::size_t frames,
             double amplitude,
             double frequency,
             double sampleRate) {
    constexpr double twoPi = 6.28318530717958647692;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const double time = static_cast<double>(start + frame) / sampleRate;
        samples[start + frame] += static_cast<float>(
            amplitude * std::sin(twoPi * frequency * time));
    }
}
}

int main() {
    using namespace pgclip;

    const AudioFormat format{48000U, 1U};
    const AnalysisConfig analysisConfig{480U, 480U, 1U};
    std::vector<float> vocal(12U * 480U, 0.0F);
    addSine(vocal, 0U, vocal.size(), 0.001, 1000.0, 48000.0);
    addSine(vocal, 2U * 480U, 8U * 480U, 0.1, 750.0, 48000.0);
    vocal[2U * 480U + 20U] += 1.0F; // phrase onset transient
    vocal[5U * 480U + 20U] += 1.0F; // isolated internal anomaly
    vocal[9U * 480U + 20U] += 1.0F; // phrase tail transient

    const auto analysis = analyzeWindowDescriptors(
        AudioBufferView{
            format, static_cast<FrameIndex>(vocal.size()), vocal.data()
        },
        analysisConfig);
    const auto snapshot = buildSharedAnalysisSnapshot(
        CaptureDescriptor{
            format, static_cast<FrameIndex>(vocal.size()),
            CaptureState::Complete, false
        },
        analysisConfig, analysis);
    check(snapshot.status == SnapshotBuildStatus::Ready && snapshot.snapshot,
          "build Clip Peak vocal fixture snapshot");

    if (snapshot.snapshot) {
        const auto activity = detectWindowActivity(
            *snapshot.snapshot,
            ActivityDetectionConfig{12.0, 6.0, 0.2, 3.0, 1U, 2U});
        check(activity.status == ActivityDetectionStatus::Ready &&
                  activity.regions.size() == 1U,
              "build phrase context for peak protection");

        const auto result = detectClipPeakEvents(
            *snapshot.snapshot, activity,
            ClipPeakConfig{2U, 6.0, 2.0, 6.0, 0.3, 1U});
        check(result.status == ClipPeakStatus::Ready &&
                  result.events.size() >= 3U,
              "detect contextual isolated peak anomalies");

        bool onsetReviewed = false;
        bool interiorAttenuated = false;
        bool tailReviewed = false;
        for (const auto& event : result.events) {
            if (event.range.start == 2U * 480U) {
                onsetReviewed = event.action == PeakEventAction::Review &&
                                event.protectedPhraseEdge;
            } else if (event.range.start == 5U * 480U) {
                interiorAttenuated =
                    event.action == PeakEventAction::Attenuate &&
                    event.suggestedAttenuationDb < 0.0 &&
                    event.suggestedAttenuationDb >= -6.0;
            } else if (event.range.start == 9U * 480U) {
                tailReviewed = event.action == PeakEventAction::Review &&
                               event.protectedPhraseEdge;
            }
        }

        check(onsetReviewed,
              "protect phrase-onset transient for manual review");
        check(interiorAttenuated,
              "propose capped local attenuation for interior anomaly");
        check(tailReviewed,
              "protect phrase-tail transient for manual review");

        const auto invalidConfig = detectClipPeakEvents(
            *snapshot.snapshot, activity, ClipPeakConfig{});
        check(invalidConfig.status == ClipPeakStatus::InvalidConfiguration,
              "reject unspecified Clip Peak parameters");
    }

    if (failures != 0) {
        std::cerr << failures << " Clip Peak test(s) failed.\n";
        return 1;
    }

    std::cout << "All Clip Peak foundation tests passed.\n";
    return 0;
}
