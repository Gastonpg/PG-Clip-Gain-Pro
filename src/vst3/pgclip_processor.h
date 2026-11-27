#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "vst3_capture_runtime.h"

#include <cstdint>

namespace Steinberg {
namespace Vst {

class PGClipGainProcessor final : public AudioEffect {
public:
    static FUnknown* createInstance(void* context);

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
    tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE;
    tresult PLUGIN_API setProcessing(TBool state) SMTG_OVERRIDE;
    tresult PLUGIN_API getControllerClassId(TUID classId) SMTG_OVERRIDE;
    tresult PLUGIN_API setBusArrangements(
        SpeakerArrangement* inputs, int32 numIns,
        SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE;
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE;
    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE;
    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE;

private:
    pgclip::Vst3CaptureRuntime captureRuntime_;
    std::uint32_t sampleRate_{0};
};

} // namespace Vst
} // namespace Steinberg
