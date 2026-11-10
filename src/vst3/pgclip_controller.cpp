#include "pgclip_controller.h"

#include "base/source/fstreamer.h"

namespace Steinberg {
namespace Vst {
namespace {

constexpr int32 kStateMagic = 0x50474350;
constexpr int32 kStateVersion = 1;

} // namespace

FUnknown* PGClipGainController::createInstance(void*) {
    return static_cast<IEditController*>(new PGClipGainController());
}

tresult PLUGIN_API PGClipGainController::initialize(FUnknown* context) {
    return EditControllerEx1::initialize(context);
}

tresult PLUGIN_API PGClipGainController::setComponentState(IBStream* state) {
    if (state == nullptr) {
        return kResultFalse;
    }

    IBStreamer streamer(state, kLittleEndian);
    int32 magic = 0;
    int32 version = 0;

    if (!streamer.readInt32(magic) || !streamer.readInt32(version)) {
        return kResultFalse;
    }

    return magic == kStateMagic && version == kStateVersion
        ? kResultOk : kResultFalse;
}

tresult PLUGIN_API PGClipGainController::setState(IBStream*) {
    return kResultOk;
}

tresult PLUGIN_API PGClipGainController::getState(IBStream*) {
    return kResultOk;
}

} // namespace Vst
} // namespace Steinberg
