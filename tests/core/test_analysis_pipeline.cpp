#include "pgclip/core/analysis_pipeline.hpp"
#include "pgclip/core/window_analysis.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void addSine(std::vector<float>& samples, std::size_t start,
             std::size_t frames, double amplitude) {
    constexpr double pi2 = 6.28318530717958647692;
    for (std::size_t i = 0; i < frames; ++i) {
        const double time = static_cast<double>(start + i) / 48000.0;
        samples[start + i] += static_cast<float>(
            amplitude * std::sin(pi2 * 750.0 * time));
    }
}
}

int main() {
    using namespace pgclip;
    const AudioFormat format{48000U, 1U};
    const AnalysisConfig analysisConfig{480U, 480U, 1U};
    std::vector<float> audio(12U * 480U, 0.0F);
    addSine(audio, 0U, audio.size(), 0.001);
    addSine(audio, 2U * 480U, 2U * 480U, 0.1);
    addSine(audio, 6U * 480U, 2U * 480U, 0.05);
    addSine(audio, 10U * 480U, 2U * 480U, 0.2);

    const auto analysis = analyzeWindowDescriptors(
        AudioBufferView{format, static_cast<FrameIndex>(audio.size()), audio.data()},
        analysisConfig);
    const auto built = buildSharedAnalysisSnapshot(
        CaptureDescriptor{format, static_cast<FrameIndex>(audio.size()),
                          CaptureState::Complete, false},
        analysisConfig, analysis);
    check(built.status == SnapshotBuildStatus::Ready && built.snapshot,
          "construct immutable shared snapshot");

    if (built.snapshot) {
        AnalysisPipelineConfig config;
        config.activity = ActivityDetectionConfig{12.0, 6.0, 0.2, 3.0, 1U, 2U};
        config.phraseProfiles = PhraseProfileConfig{2U, 0.5};
        config.adaptiveReference = AdaptiveReferenceConfig{2U, 0.5};
        config.clipGain = ClipGainRecommendationConfig{0.5, 6.0, 6.0, 0.5};
        config.clipGainCurve = ClipGainCurveConfig{120U, 120U};
        config.clipPeak = ClipPeakConfig{2U, 6.0, 2.0, 6.0, 0.3, 1U};
        config.clipPeakCurve = ClipPeakCurveConfig{120U, 120U};
        config.combination = CurveCombinationConfig{6.0, 12.0};

        const auto result = buildAnalysisPipeline(*built.snapshot, config);
        check(result.status == AnalysisPipelineStatus::Ready,
              "run both independent engines and safety coordinator");
        check(result.activity.windows.size() == built.snapshot->windows().size(),
              "share one activity map across both engines");
        check(result.clipGainRecommendations.status ==
                  ClipGainRecommendationStatus::Ready,
              "produce independent Clip Gain recommendations");
        check(result.clipPeakEvents.status == ClipPeakStatus::Ready,
              "produce independent Clip Peak events");
        check(!result.combinedCurve.points.empty(),
              "publish a combined static curve for downstream review");

        config.combination.maximumCombinedBoostDb = 0.0;
        const auto limited = buildAnalysisPipeline(*built.snapshot, config);
        check(limited.status == AnalysisPipelineStatus::Ready,
              "complete coordination when the safety ceiling is tightened");
        bool boostWithinLimit = true;
        for (const auto& point : limited.combinedCurve.points) {
            if (point.gainDb > 1.0e-9) {
                boostWithinLimit = false;
            }
        }
        check(boostWithinLimit,
              "clamp combined boost to the configured safety ceiling");
    }

    if (failures != 0) {
        std::cerr << failures << " analysis pipeline test(s) failed.\n";
        return 1;
    }
    std::cout << "All analysis pipeline tests passed.\n";
    return 0;
}
