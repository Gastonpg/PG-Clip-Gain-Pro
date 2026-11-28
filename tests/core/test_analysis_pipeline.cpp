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
        check(result.judges.evaluated,
              "run all five specialist judges and final arbiter");
        check(std::isfinite(result.judges.finalArbiter.score),
              "publish a finite final arbitration score");
        if (result.judges.approved) {
            check(!result.judges.approvedCurve.points.empty(),
                  "publish only the judge-approved static curve");
        } else {
            check(result.judges.approvedCurve.points.empty(),
                  "withhold the curve when the arbiter rejects it");
        }

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


    // End-to-end regression: a short pause bridged into one phrase must
    // remain at unity gain in the final coordinated curve.
    std::vector<float> pauseAudio(12U * 480U, 0.0F);
    addSine(pauseAudio, 0U, pauseAudio.size(), 0.001);
    addSine(pauseAudio, 1U * 480U, 2U * 480U, 0.1);
    addSine(pauseAudio, 4U * 480U, 2U * 480U, 0.1);
    addSine(pauseAudio, 8U * 480U, 2U * 480U, 0.08);
    const auto pauseAnalysis = analyzeWindowDescriptors(
        AudioBufferView{
            format, static_cast<FrameIndex>(pauseAudio.size()),
            pauseAudio.data()
        },
        analysisConfig);
    const auto pauseSnapshot = buildSharedAnalysisSnapshot(
        CaptureDescriptor{
            format, static_cast<FrameIndex>(pauseAudio.size()),
            CaptureState::Complete, false
        },
        analysisConfig, pauseAnalysis);
    check(pauseSnapshot.status == SnapshotBuildStatus::Ready &&
              static_cast<bool>(pauseSnapshot.snapshot),
          "build end-to-end bridged-pause fixture");

    if (pauseSnapshot.snapshot) {
        AnalysisPipelineConfig pauseConfig;
        pauseConfig.activity = ActivityDetectionConfig{
            12.0, 6.0, 0.2, 3.0, 1U, 2U
        };
        pauseConfig.phraseProfiles = PhraseProfileConfig{2U, 0.5};
        pauseConfig.adaptiveReference = AdaptiveReferenceConfig{2U, 0.5};
        pauseConfig.clipGain = ClipGainRecommendationConfig{
            0.5, 6.0, 6.0, 0.5
        };
        pauseConfig.clipGainCurve = ClipGainCurveConfig{120U, 120U};
        pauseConfig.clipPeak = ClipPeakConfig{
            2U, 6.0, 2.0, 6.0, 0.3, 1U
        };
        pauseConfig.clipPeakCurve = ClipPeakCurveConfig{120U, 120U};
        pauseConfig.combination = CurveCombinationConfig{6.0, 12.0};

        const auto pauseResult = buildAnalysisPipeline(
            *pauseSnapshot.snapshot, pauseConfig);
        check(pauseResult.status == AnalysisPipelineStatus::Ready,
              "complete full analysis pipeline with a bridged internal pause");

        bool pauseWasProtected = false;
        if (pauseResult.status == AnalysisPipelineStatus::Ready) {
            for (const auto& recommendation :
                 pauseResult.clipGainRecommendations.recommendations) {
                for (const auto& protectedRange :
                     recommendation.protectedRanges) {
                    if (protectedRange.start == 1440U &&
                        protectedRange.end == 1920U) {
                        pauseWasProtected = true;
                    }
                }
            }
        }
        check(pauseWasProtected,
              "carry the detected pause through Clip Gain recommendations");

        bool finalCurveKeepsPauseAtUnity = false;
        if (pauseResult.status == AnalysisPipelineStatus::Ready) {
            const auto& points = pauseResult.combinedCurve.points;
            bool zeroAtPauseStart = false;
            bool zeroAtPauseEnd = false;
            for (const auto& point : points) {
                if (point.frame == 1440U &&
                    std::abs(point.gainDb) < 1.0e-9) {
                    zeroAtPauseStart = true;
                }
                if (point.frame == 1920U &&
                    std::abs(point.gainDb) < 1.0e-9) {
                    zeroAtPauseEnd = true;
                }
            }
            finalCurveKeepsPauseAtUnity =
                zeroAtPauseStart && zeroAtPauseEnd;
        }
        check(finalCurveKeepsPauseAtUnity,
              "preserve unity gain at both boundaries in the final combined curve");
    }

    // Specialist-judge regression fixtures: approve a coherent phrase edit,
    // reject a correction in the wrong musical direction, and protect the
    // complete interpolated shape across a bridged pause.
    {
        PhraseProfile phrase;
        phrase.range = {0U, 200U};
        phrase.rmsLevelDb = -20.0;
        phrase.peakDb = -13.9794000867;
        phrase.crestFactor = 2.0;
        phrase.confidence = 0.95;
        phrase.eligibleForReference = true;

        AdaptiveReferenceResult reference;
        reference.status = AdaptiveReferenceStatus::Ready;
        reference.referenceRmsDb = -18.0;
        reference.medianAbsoluteDeviationDb = 1.0;
        reference.eligiblePhraseCount = 3U;

        PhraseGainRecommendation recommendation;
        recommendation.range = phrase.range;
        recommendation.correctionDb = 2.0;
        recommendation.confidence = 0.95;
        recommendation.action = GainRecommendationAction::Adjust;

        ClipPeakResult noPeaks;
        noPeaks.status = ClipPeakStatus::Ready;

        GainCurve coherent;
        coherent.points = {{0U, 0.0}, {100U, 2.0}, {200U, 0.0}};
        const auto approved = runFiveJudges(
            {phrase}, reference, {recommendation}, noPeaks,
            coherent, 200U);
        check(approved.evaluated && approved.approved,
              "approve a coherent, correctly directed phrase correction");
        check(approved.approvedCurve.points.size() == coherent.points.size(),
              "publish the complete judge-approved static curve");

        recommendation.correctionDb = -2.0;
        const auto wrongDirection = runFiveJudges(
            {phrase}, reference, {recommendation}, noPeaks,
            coherent, 200U);
        check(wrongDirection.evaluated && !wrongDirection.approved &&
                  wrongDirection.adaptiveLeveling.veto,
              "veto a phrase correction that moves away from the adaptive reference");

        phrase.protectedRanges.push_back({80U, 120U});
        recommendation.correctionDb = 2.0;
        GainCurve crossingPause;
        crossingPause.points = {{0U, 2.0}, {200U, 0.0}};
        const auto protectedPause = runFiveJudges(
            {phrase}, reference, {recommendation}, noPeaks,
            crossingPause, 200U);
        check(protectedPause.evaluated && !protectedPause.approved &&
                  protectedPause.globalCoherence.veto,
              "veto interpolated gain that crosses a protected pause");
    }

    if (failures != 0) {
        std::cerr << failures << " analysis pipeline test(s) failed.\n";
        return 1;
    }
    std::cout << "All analysis pipeline tests passed.\n";
    return 0;
}
