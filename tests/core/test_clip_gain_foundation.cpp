#include "pgclip/core/clip_gain_foundation.hpp"
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
    const ActivityDetectionConfig activityConfig{
        12.0, 6.0, 0.2, 3.0, 1U, 2U
    };

    std::vector<float> vocalTake(12U * 480U, 0.0F);
    addSine(vocalTake, 0U, vocalTake.size(),
            0.001, 1000.0, 48000.0);
    addSine(vocalTake, 2U * 480U, 2U * 480U,
            0.1, 750.0, 48000.0);
    addSine(vocalTake, 6U * 480U, 2U * 480U,
            0.05, 750.0, 48000.0);
    addSine(vocalTake, 10U * 480U, 2U * 480U,
            0.2, 750.0, 48000.0);

    const auto analysis = analyzeWindowDescriptors(
        AudioBufferView{
            format, static_cast<FrameIndex>(vocalTake.size()),
            vocalTake.data()
        },
        analysisConfig);
    const auto snapshot = buildSharedAnalysisSnapshot(
        CaptureDescriptor{
            format, static_cast<FrameIndex>(vocalTake.size()),
            CaptureState::Complete, false
        },
        analysisConfig,
        analysis);
    check(snapshot.status == SnapshotBuildStatus::Ready && snapshot.snapshot,
          "build shared snapshot from multi-level vocal fixture");

    if (snapshot.snapshot) {
        const auto activity = detectWindowActivity(
            *snapshot.snapshot, activityConfig);
        check(activity.status == ActivityDetectionStatus::Ready &&
                  activity.regions.size() == 3U,
              "detect three distinct phrase-region candidates");

        const auto phraseProfiles = analyzePhraseProfiles(
            *snapshot.snapshot, activity,
            PhraseProfileConfig{2U, 0.5});
        check(phraseProfiles.status == PhraseProfileStatus::Ready &&
                  phraseProfiles.profiles.size() == 3U,
              "measure robust profiles for all candidate phrases");

        const auto invalidProfileConfig = analyzePhraseProfiles(
            *snapshot.snapshot, activity, PhraseProfileConfig{0U, 0.5});
        check(invalidProfileConfig.status == PhraseProfileStatus::InvalidConfiguration,
              "reject zero minimum active windows");

        if (phraseProfiles.profiles.size() == 3U) {
            check(phraseProfiles.profiles[0U].eligibleForReference &&
                      phraseProfiles.profiles[1U].eligibleForReference &&
                      phraseProfiles.profiles[2U].eligibleForReference,
                  "require sufficient active windows and confidence");

            const auto reference = selectAdaptiveReference(
                phraseProfiles.profiles,
                AdaptiveReferenceConfig{2U, 0.5});
            check(reference.status == AdaptiveReferenceStatus::Ready,
                  "select adaptive reference from the recording itself");
            if (reference.status == AdaptiveReferenceStatus::Ready) {
                check(std::abs(reference.referenceRmsDb -
                               phraseProfiles.profiles[0U].rmsLevelDb) < 0.2,
                      "choose median phrase level rather than fixed dBFS target");
                check(reference.medianAbsoluteDeviationDb > 5.0,
                      "report robust phrase-level dispersion");

                const auto recommendations = recommendClipGain(
                    phraseProfiles.profiles,
                    reference,
                    ClipGainRecommendationConfig{0.5, 6.0, 6.0, 0.5});
                check(recommendations.status ==
                          ClipGainRecommendationStatus::Ready &&
                          recommendations.recommendations.size() == 3U,
                      "create editable phrase-level relative recommendations");
                if (recommendations.recommendations.size() == 3U) {
                    check(recommendations.recommendations[0U].action ==
                              GainRecommendationAction::Keep &&
                              std::abs(recommendations.recommendations[0U].correctionDb) <
                                  0.05,
                          "keep phrase already at the internal reference");
                    check(recommendations.recommendations[1U].action ==
                              GainRecommendationAction::Adjust &&
                              recommendations.recommendations[1U].correctionDb > 2.5 &&
                              recommendations.recommendations[1U].correctionDb < 3.5,
                          "raise quieter phrase by a moderated relative amount");
                    check(recommendations.recommendations[2U].action ==
                              GainRecommendationAction::Adjust &&
                              recommendations.recommendations[2U].correctionDb < -2.5 &&
                              recommendations.recommendations[2U].correctionDb > -3.5,
                          "reduce louder phrase by a moderated relative amount");
                }

                auto uncertainProfiles = phraseProfiles.profiles;
                uncertainProfiles[1U].eligibleForReference = false;
                const auto abstained = recommendClipGain(
                    uncertainProfiles,
                    reference,
                    ClipGainRecommendationConfig{0.5, 6.0, 6.0, 0.5});
                check(abstained.status == ClipGainRecommendationStatus::Ready &&
                          abstained.recommendations[1U].action ==
                              GainRecommendationAction::Abstain,
                      "abstain rather than adjust a phrase marked ineligible");

                const auto curve = buildClipGainCurve(
                    recommendations.recommendations,
                    static_cast<FrameIndex>(vocalTake.size()),
                    ClipGainCurveConfig{120U, 120U});
                check(curve.status == ClipGainCurveStatus::Ready &&
                          curve.curve.points.size() == 8U,
                      "build separate static phrase ramps with zero-gain gaps");
                if (curve.status == ClipGainCurveStatus::Ready) {
                    check(!validate(curve.curve) &&
                              curve.curve.points[1U].frame == 3000U &&
                              curve.curve.points[1U].gainDb > 2.5 &&
                              curve.curve.points[5U].gainDb < -2.5,
                          "validate ordered curve points and relative phrase gains");
                }

                PhraseGainRecommendation pauseProtected;
                pauseProtected.range = FrameRange{0U, 1000U};
                pauseProtected.correctionDb = 3.0;
                pauseProtected.confidence = 1.0;
                pauseProtected.action = GainRecommendationAction::Adjust;
                pauseProtected.protectedRanges.push_back(FrameRange{400U, 600U});
                const auto pauseProtectedCurve = buildClipGainCurve(
                    std::vector<PhraseGainRecommendation>{pauseProtected},
                    1000U, ClipGainCurveConfig{100U, 100U});
                check(pauseProtectedCurve.status == ClipGainCurveStatus::Ready,
                      "build phrase gain curve around a protected low-level pause");
                if (pauseProtectedCurve.status == ClipGainCurveStatus::Ready) {
                    const auto& points = pauseProtectedCurve.curve.points;
                    bool hasZeroPauseBoundaries = false;
                    bool hasGainBeforePause = false;
                    bool hasGainAfterPause = false;
                    for (const auto& point : points) {
                        if ((point.frame == 400U || point.frame == 600U) &&
                            std::abs(point.gainDb) < 1.0e-9) {
                            hasZeroPauseBoundaries = true;
                        }
                        if (point.frame == 100U && point.gainDb > 2.9) {
                            hasGainBeforePause = true;
                        }
                        if (point.frame == 700U && point.gainDb > 2.9) {
                            hasGainAfterPause = true;
                        }
                    }
                    check(hasZeroPauseBoundaries && hasGainBeforePause &&
                              hasGainAfterPause,
                          "return to unity across the protected pause and resume smoothly");
                }

                auto invalidProtected = pauseProtected;
                invalidProtected.protectedRanges = {FrameRange{900U, 1100U}};
                const auto invalidProtectedCurve = buildClipGainCurve(
                    std::vector<PhraseGainRecommendation>{invalidProtected},
                    1000U, ClipGainCurveConfig{100U, 100U});
                check(invalidProtectedCurve.status ==
                          ClipGainCurveStatus::InvalidRecommendations,
                      "reject protected ranges extending beyond their phrase");

                const std::vector<PhraseGainRecommendation> shortAdjacent{
                    PhraseGainRecommendation{
                        FrameRange{0U, 2U}, 2.0, 1.0,
                        GainRecommendationAction::Adjust
                    },
                    PhraseGainRecommendation{
                        FrameRange{2U, 4U}, -1.0, 1.0,
                        GainRecommendationAction::Adjust
                    }
                };
                const auto shortCurve = buildClipGainCurve(
                    shortAdjacent, 4U, ClipGainCurveConfig{120U, 120U});
                check(shortCurve.status == ClipGainCurveStatus::Ready &&
                          shortCurve.curve.points.size() == 5U &&
                          shortCurve.curve.points[2U].frame == 2U &&
                          shortCurve.curve.points[2U].gainDb == 0.0,
                      "handle two-frame phrases and deduplicate contiguous zero-gain boundary");

                auto overlappingKeep = recommendations.recommendations;
                overlappingKeep[0U].range.end = 3000U;
                const auto overlapWithKeep = buildClipGainCurve(
                    overlappingKeep,
                    static_cast<FrameIndex>(vocalTake.size()),
                    ClipGainCurveConfig{120U, 120U});
                check(overlapWithKeep.status ==
                          ClipGainCurveStatus::OverlappingRegions,
                      "do not apply gain through an overlapping phrase marked Keep");

                auto overlappingRecommendations = recommendations.recommendations;
                overlappingRecommendations[2U].range.start = 3800U;
                const auto overlapCurve = buildClipGainCurve(
                    overlappingRecommendations,
                    static_cast<FrameIndex>(vocalTake.size()),
                    ClipGainCurveConfig{120U, 120U});
                check(overlapCurve.status == ClipGainCurveStatus::OverlappingRegions,
                      "reject overlapping phrase gain regions");

                const auto zeroStrength = recommendClipGain(
                    phraseProfiles.profiles,
                    reference,
                    ClipGainRecommendationConfig{0.0, 6.0, 6.0, 0.5});
                check(zeroStrength.status == ClipGainRecommendationStatus::Ready &&
                          zeroStrength.recommendations[1U].action ==
                              GainRecommendationAction::Keep,
                      "zero correction strength preserves all phrase levels");
            }

            const auto insufficient = selectAdaptiveReference(
                phraseProfiles.profiles,
                AdaptiveReferenceConfig{4U, 0.5});
            check(insufficient.status == AdaptiveReferenceStatus::NotEnoughPhrases,
                  "refuse reference when too few eligible phrases exist");

            const auto noRecommendation = recommendClipGain(
                phraseProfiles.profiles,
                insufficient,
                ClipGainRecommendationConfig{0.5, 6.0, 6.0, 0.5});
            check(noRecommendation.status ==
                      ClipGainRecommendationStatus::ReferenceUnavailable,
                  "do not generate corrections without a valid reference");
        }
    }

    if (failures != 0) {
        std::cerr << failures << " Clip Gain foundation test(s) failed.\n";
        return 1;
    }

    std::cout << "All Clip Gain foundation tests passed.\n";
    return 0;
}
