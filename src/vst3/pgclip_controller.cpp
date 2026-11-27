#include "pgclip_controller.h"
#include "pgclip_cids.h"
#include "pgclip_editor_view.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/fstrdefs.h"

#include <cstring>

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
    const tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk) {
        return result;
    }

    parameters.addParameter(
        STR16("Capture"), STR16(""), 1, 0.0,
        ParameterInfo::kCanAutomate, kCaptureEnableParameterId);
    return kResultOk;
}

IPlugView* PLUGIN_API PGClipGainController::createView(FIDString name) {
    if (name == nullptr || std::strcmp(name, ViewType::kEditor) != 0) {
        return nullptr;
    }
    return new PGClipGainEditorView(this);
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

tresult PLUGIN_API PGClipGainController::setState(IBStream* state) {
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

tresult PLUGIN_API PGClipGainController::getState(IBStream* state) {
    if (state == nullptr) {
        return kResultFalse;
    }

    IBStreamer streamer(state, kLittleEndian);
    if (!streamer.writeInt32(kStateMagic) ||
        !streamer.writeInt32(kStateVersion)) {
        return kResultFalse;
    }
    return kResultOk;
}

} // namespace Vst
} // namespace Steinberg
