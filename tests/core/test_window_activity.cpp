#include "pgclip/core/window_activity.hpp"
#include "pgclip/core/window_analysis.hpp"

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

pgclip::SnapshotBuildResult makeSnapshot(
    const std::vector<float>& samples,
    const pgclip::AudioFormat& format,
    const pgclip::AnalysisConfig& analysisConfig) {
    const auto analysis = pgclip::analyzeWindowDescriptors(
        pgclip::AudioBufferView{
            format, static_cast<pgclip::FrameIndex>(
                        samples.size() / format.channelCount),
            samples.data()
        },
        analysisConfig);

    return pgclip::buildSharedAnalysisSnapshot(
        pgclip::CaptureDescriptor{
            format,
            static_cast<pgclip::FrameIndex>(
                samples.size() / format.channelCount),
            pgclip::CaptureState::Complete,
            false
        },
        analysisConfig,
        analysis);
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
    const ActivityDetectionConfig detectorConfig{
        12.0, 6.0, 0.2, 3.0, 1U, 2U
    };

    std::vector<float> backgroundAndVoice(8U * 480U, 0.0F);
    addSine(backgroundAndVoice, 0U, backgroundAndVoice.size(),
            0.001, 1000.0, 48000.0);
    addSine(backgroundAndVoice, 2U * 480U, 2U * 480U,
            0.1, 750.0, 48000.0);
    addSine(backgroundAndVoice, 6U * 480U, 480U,
            0.1, 750.0, 48000.0);

    const auto built = makeSnapshot(
        backgroundAndVoice, format, analysisConfig);
    check(built.status == SnapshotBuildStatus::Ready && built.snapshot,
          "build activity fixture snapshot");

    if (built.snapshot) {
        const auto result = detectWindowActivity(*built.snapshot, detectorConfig);
        check(result.status == ActivityDetectionStatus::Ready,
              "detect candidate activity with noise-relative thresholds");
        check(result.windows.size() == 8U,
              "return one activity result per analysis window");
        if (result.windows.size() == 8U) {
            check(result.windows[2U].state == WindowActivityState::Active &&
                      result.windows[3U].state == WindowActivityState::Active,
                  "detect strong voice-like burst above background floor");
            check(result.windows[0U].state == WindowActivityState::BelowThreshold &&
                      result.windows[1U].state == WindowActivityState::BelowThreshold,
                  "leave low-level background below activity threshold");
            check(result.windows[6U].state == WindowActivityState::Active,
                  "detect a separate later vocal candidate");
        }
    }

    std::vector<float> bridged(6U * 480U, 0.0F);
    addSine(bridged, 0U, bridged.size(), 0.001, 1000.0, 48000.0);
    addSine(bridged, 1U * 480U, 2U * 480U, 0.1, 750.0, 48000.0);
    addSine(bridged, 4U * 480U, 2U * 480U, 0.1, 750.0, 48000.0);
    const auto bridgedSnapshot = makeSnapshot(bridged, format, analysisConfig);
    if (bridgedSnapshot.snapshot) {
        const auto result = detectWindowActivity(
            *bridgedSnapshot.snapshot, detectorConfig);
        check(result.windows[3U].state == WindowActivityState::BridgedPause,
              "bridge one short internal pause without deleting its position");
    } else {
        check(false, "build short-pause fixture snapshot");
    }

    std::vector<float> singleTransient(8U * 480U, 0.0F);
    addSine(singleTransient, 0U, singleTransient.size(),
            0.001, 1000.0, 48000.0);
    addSine(singleTransient, 4U * 480U, 480U,
            0.1, 750.0, 48000.0);
    const auto transientSnapshot = makeSnapshot(
        singleTransient, format, analysisConfig);
    if (transientSnapshot.snapshot) {
        const auto result = detectWindowActivity(
            *transientSnapshot.snapshot, detectorConfig);
        check(result.status == ActivityDetectionStatus::InsufficientContrast &&
                  result.windows[4U].state == WindowActivityState::Uncertain,
              "abstain on one isolated energy spike instead of declaring a phrase");
    } else {
        check(false, "build isolated transient fixture snapshot");
    }

    std::vector<float> uniformVoice(4U * 480U, 0.0F);
    addSine(uniformVoice, 0U, uniformVoice.size(),
            0.1, 750.0, 48000.0);
    const auto uniformSnapshot = makeSnapshot(
        uniformVoice, format, analysisConfig);
    if (uniformSnapshot.snapshot) {
        const auto result = detectWindowActivity(
            *uniformSnapshot.snapshot, detectorConfig);
        check(result.status == ActivityDetectionStatus::InsufficientContrast,
              "abstain when a continuous uniform signal provides no noise contrast");
        check(result.windows[0U].state == WindowActivityState::Uncertain,
              "mark uniform activity as uncertain rather than silence");
    } else {
        check(false, "build uniform signal fixture snapshot");
    }

    std::vector<float> silence(4U * 480U, 0.0F);
    const auto silenceSnapshot = makeSnapshot(
        silence, format, analysisConfig);
    if (silenceSnapshot.snapshot) {
        const auto result = detectWindowActivity(
            *silenceSnapshot.snapshot, detectorConfig);
        check(result.status == ActivityDetectionStatus::NoUsableWindows &&
                  result.silentWindows == 4U,
              "recognize digital silence without fabricating activity");
    } else {
        check(false, "build silence fixture snapshot");
    }

    std::vector<float> invalidSamples(4U * 480U, 0.0F);
    addSine(invalidSamples, 0U, invalidSamples.size(),
            0.1, 750.0, 48000.0);
    invalidSamples[2U * 480U + 4U] =
        std::numeric_limits<float>::quiet_NaN();
    const auto invalidSnapshot = makeSnapshot(
        invalidSamples, format, analysisConfig);
    if (invalidSnapshot.snapshot) {
        const auto result = detectWindowActivity(
            *invalidSnapshot.snapshot, detectorConfig);
        check(result.invalidWindows == 1U &&
                  result.windows[2U].state == WindowActivityState::Invalid,
              "isolate invalid windows without contaminating other candidates");
    } else {
        check(false, "build invalid-window fixture snapshot");
    }

    const ActivityDetectionConfig invalidConfig{};
    if (built.snapshot) {
        const auto result = detectWindowActivity(
            *built.snapshot, invalidConfig);
        check(result.status == ActivityDetectionStatus::InvalidConfiguration,
              "reject unspecified activity detector parameters");
    }

    if (failures != 0) {
        std::cerr << failures << " activity detector test(s) failed.\n";
        return 1;
    }

    std::cout << "All window activity tests passed.\n";
    return 0;
}
