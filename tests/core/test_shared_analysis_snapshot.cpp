#include "pgclip/core/shared_analysis_snapshot.hpp"
#include "pgclip/core/synthetic_signals.hpp"

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
}

int main() {
    using namespace pgclip;
    using namespace pgclip::test;

    const AudioFormat format{48000U, 1U};
    const AnalysisConfig config{128U, 64U, 1U};
    const auto signal = makeSine(format, 256U, 500.0, 0.4);

    if (signal) {
        const auto analysis = analyzeWindowDescriptors(
            AudioBufferView{format, signal->frameCount,
                            signal->interleaved.data()},
            config);

        const CaptureDescriptor complete{
            format, 256U, CaptureState::Complete, false
        };
        auto built = buildSharedAnalysisSnapshot(complete, config, analysis);
        check(built.status == SnapshotBuildStatus::Ready && built.snapshot,
              "build snapshot from complete capture and analysis");
        if (built.snapshot) {
            check(built.snapshot->version() == 1U,
                  "snapshot has explicit version");
            check(built.snapshot->totalFrames() == 256U,
                  "snapshot preserves captured frame count");
            check(built.snapshot->windows().size() == 4U,
                  "snapshot exposes immutable descriptor collection");
        }

        const CaptureDescriptor partial{
            format, 128U, CaptureState::Partial, false
        };
        const auto rejected =
            buildSharedAnalysisSnapshot(partial, config, analysis);
        check(rejected.status == SnapshotBuildStatus::IncompleteCapture &&
                  !rejected.snapshot,
              "reject partial capture for global snapshot");

        const CaptureDescriptor discontinuous{
            format, 256U, CaptureState::Discontinuous, true
        };
        const auto rejectedGap =
            buildSharedAnalysisSnapshot(discontinuous, config, analysis);
        check(rejectedGap.status == SnapshotBuildStatus::IncompleteCapture,
              "reject discontinuous capture for global snapshot");

        auto incompleteAnalysis = analysis;
        incompleteAnalysis.windows.pop_back();
        const auto rejectedCoverage =
            buildSharedAnalysisSnapshot(complete, config,
                                        std::move(incompleteAnalysis));
        check(rejectedCoverage.status == SnapshotBuildStatus::CoverageMismatch,
              "reject descriptor set missing tail coverage");
    }

    std::vector<float> invalidSamples(128U, 0.25F);
    const auto invalidAnalysis = analyzeWindowDescriptors(
        AudioBufferView{format, 128U, invalidSamples.data()},
        AnalysisConfig{128U, 128U, 1U});
    if (!invalidAnalysis.windows.empty()) {
        auto malformed = invalidAnalysis;
        malformed.windows[0].channels[0].rms = 2.0;
        const auto rejected = buildSharedAnalysisSnapshot(
            CaptureDescriptor{format, 128U, CaptureState::Complete, false},
            AnalysisConfig{128U, 128U, 1U}, std::move(malformed));
        check(rejected.status == SnapshotBuildStatus::InvalidDescriptor,
              "reject impossible RMS greater than peak");
    }

    if (failures != 0) {
        std::cerr << failures << " snapshot test(s) failed.\n";
        return 1;
    }

    std::cout << "All shared snapshot tests passed.\n";
    return 0;
}
