#include "pgclip/core/gain_curve_processor.hpp"

#include <array>
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

    const GainCurve curve{{
        GainPoint{100U, 0.0},
        GainPoint{102U, 6.0},
        GainPoint{104U, 0.0}
    }};
    const auto runtime = prepareGainCurveRuntime(curve);
    check(runtime.status == GainCurveRuntimeStatus::Ready &&
              runtime.runtime.points().size() == 3U,
          "prepare immutable linear-gain runtime outside the callback");

    if (runtime.status == GainCurveRuntimeStatus::Ready) {
        std::array<float, 8> monoSamples{
            1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F
        };
        const std::array<void*, 1> monoChannels{monoSamples.data()};
        const auto status = applyGainCurveToBlock(
            MutableAudioBlockView{
                monoChannels.data(),
                98U, 8U, 1U, AudioSampleFormat::Float32
            },
            runtime.runtime);
        check(status == GainCurveApplyStatus::Applied,
              "apply static curve to float32 block");
        check(std::abs(monoSamples[0] - 1.0F) < 1.0e-6F &&
                  std::abs(monoSamples[2] - 1.0F) < 1.0e-6F &&
                  std::abs(monoSamples[4] - 1.995262F) < 0.002F &&
                  std::abs(monoSamples[6] - 1.0F) < 1.0e-6F,
              "apply unity outside the curve and relative gain inside it");
        check(monoSamples[3] > 1.4F && monoSamples[3] < 1.6F,
              "use smooth amplitude interpolation between control points");

        std::array<double, 2> left{0.5, 0.5};
        std::array<double, 2> right{-0.5, -0.5};
        const std::array<void*, 2> stereoChannels{
            left.data(), right.data()
        };
        const auto stereoStatus = applyGainCurveToBlock(
            MutableAudioBlockView{
                stereoChannels.data(),
                101U, 2U, 2U, AudioSampleFormat::Float64
            },
            runtime.runtime);
        check(stereoStatus == GainCurveApplyStatus::Applied &&
                  left[0] > 0.7 && right[0] < -0.7,
              "apply identical curve to stereo float64 channels");
    }

    const GainCurve clipGainCurve{{
        GainPoint{0U, 0.0}, GainPoint{5U, 6.0}, GainPoint{10U, 0.0}
    }};
    const GainCurve clipPeakCurve{{
        GainPoint{0U, 0.0}, GainPoint{5U, -6.0}, GainPoint{10U, 0.0}
    }};
    const auto neutralCombined = combineStaticGainCurves(
        clipGainCurve, clipPeakCurve, 10U,
        CurveCombinationConfig{6.0, 6.0});
    check(neutralCombined.status == CurveCombinationStatus::Ready &&
              neutralCombined.curve.points.size() == 5U &&
              neutralCombined.curve.points[2U].frame == 5U &&
              std::abs(neutralCombined.curve.points[2U].gainDb) < 1.0e-6,
          "combine opposite Clip Gain and Clip Peak corrections in linear gain");

    const GainCurve extraCut{{
        GainPoint{0U, 0.0}, GainPoint{5U, -6.0}, GainPoint{10U, 0.0}
    }};
    const auto cappedCombined = combineStaticGainCurves(
        extraCut, clipPeakCurve, 10U,
        CurveCombinationConfig{6.0, 6.0});
    check(cappedCombined.status == CurveCombinationStatus::Ready &&
              std::abs(cappedCombined.curve.points[2U].gainDb + 6.0) < 1.0e-6,
          "cap the combined attenuation to the shared safety limit");

    const GainCurve outsideCurve{{
        GainPoint{0U, 0.0}, GainPoint{11U, 3.0}
    }};
    check(combineStaticGainCurves(
              outsideCurve, GainCurve{}, 10U,
              CurveCombinationConfig{6.0, 6.0}).status ==
              CurveCombinationStatus::InvalidInputCurve,
          "reject combined curve points outside the capture timeline");

    const GainCurve emptyCurve{};
    const auto emptyRuntime = prepareGainCurveRuntime(emptyCurve);
    std::array<float, 2> unchanged{0.25F, -0.25F};
    const std::array<void*, 1> unchangedChannels{unchanged.data()};
    const auto emptyStatus = applyGainCurveToBlock(
        MutableAudioBlockView{
            unchangedChannels.data(),
            0U, 2U, 1U, AudioSampleFormat::Float32
        },
        emptyRuntime.runtime);
    check(emptyRuntime.status == GainCurveRuntimeStatus::Ready &&
              emptyStatus == GainCurveApplyStatus::NoCurve &&
              unchanged[0] == 0.25F && unchanged[1] == -0.25F,
          "leave audio untouched when no curve exists");

    const GainCurve singlePoint{{GainPoint{0U, 3.0}}};
    check(prepareGainCurveRuntime(singlePoint).status ==
              GainCurveRuntimeStatus::InvalidCurve,
          "reject one-point runtime curve");

    const auto invalidPointerStatus = applyGainCurveToBlock(
        MutableAudioBlockView{nullptr, 0U, 2U, 1U,
                              AudioSampleFormat::Float32},
        runtime.runtime);
    check(invalidPointerStatus == GainCurveApplyStatus::InvalidBlock,
          "reject missing mutable channel pointer");

    std::array<float, 2> overflowSamples{1.0F, 1.0F};
    const std::array<void*, 1> overflowChannels{overflowSamples.data()};
    const auto overflowStatus = applyGainCurveToBlock(
        MutableAudioBlockView{
            overflowChannels.data(),
            std::numeric_limits<FrameIndex>::max(), 2U, 1U,
            AudioSampleFormat::Float32
        },
        runtime.runtime);
    check(overflowStatus == GainCurveApplyStatus::InvalidBlock,
          "reject absolute frame index overflow");

    if (failures != 0) {
        std::cerr << failures << " gain curve processing test(s) failed.\n";
        return 1;
    }

    std::cout << "All gain curve processing tests passed.\n";
    return 0;
}
