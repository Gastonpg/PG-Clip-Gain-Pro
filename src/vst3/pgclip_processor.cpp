#include "pgclip_processor.h"
#include "pgclip_cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <cstdint>
#include <cstring>

namespace Steinberg {
namespace Vst {
namespace {

constexpr int32 kStateMagic = 0x50474350; // "PGCP"
constexpr int32 kStateVersion = 1;

template <typename Sample>
void copyAudioBus(const AudioBusBuffers& input,
                  AudioBusBuffers& output,
                  int32 channelCount,
                  int32 sampleCount) noexcept {
    for (int32 channel = 0; channel < channelCount; ++channel) {
        auto* destination = output.channelBuffers32
            ? reinterpret_cast<Sample*>(output.channelBuffers32[channel])
            : nullptr;
        const auto* source = input.channelBuffers32
            ? reinterpret_cast<const Sample*>(input.channelBuffers32[channel])
            : nullptr;

        if constexpr (sizeof(Sample) == sizeof(double)) {
            destination = output.channelBuffers64
                ? output.channelBuffers64[channel] : nullptr;
            source = input.channelBuffers64
                ? input.channelBuffers64[channel] : nullptr;
        }

        if (destination == nullptr) {
            continue;
        }

        const bool channelIsSilent =
            (input.silenceFlags & (static_cast<uint64>(1) << channel)) != 0U;

        if (channelIsSilent || source == nullptr) {
            std::memset(destination, 0,
                        static_cast<std::size_t>(sampleCount) * sizeof(Sample));
        } else if (destination != source) {
            std::memcpy(destination, source,
                        static_cast<std::size_t>(sampleCount) * sizeof(Sample));
        }
    }
}

} // namespace

FUnknown* PGClipGainProcessor::createInstance(void*) {
    return static_cast<IAudioProcessor*>(new PGClipGainProcessor());
}

tresult PLUGIN_API PGClipGainProcessor::initialize(FUnknown* context) {
    const tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) {
        return result;
    }

    setControllerClass(PGClipGainControllerUID);
    addAudioInput(STR16("Audio In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Audio Out"), SpeakerArr::kStereo);
    return kResultOk;
}

tresult PLUGIN_API PGClipGainProcessor::getControllerClassId(TUID classId) {
    PGClipGainControllerUID.toTUID(classId);
    return kResultOk;
}

tresult PLUGIN_API PGClipGainProcessor::setBusArrangements(
    SpeakerArrangement* inputs, int32 numIns,
    SpeakerArrangement* outputs, int32 numOuts) {
    if (inputs == nullptr || outputs == nullptr ||
        numIns != 1 || numOuts != 1) {
        return kResultFalse;
    }

    const int32 inputChannels = SpeakerArr::getChannelCount(inputs[0]);
    const int32 outputChannels = SpeakerArr::getChannelCount(outputs[0]);

    if (inputChannels != outputChannels ||
        (inputChannels != 1 && inputChannels != 2)) {
        return kResultFalse;
    }

    auto* inputBus = getAudioInput(0);
    auto* outputBus = getAudioOutput(0);
    if (inputBus == nullptr || outputBus == nullptr) {
        return kResultFalse;
    }

    inputBus->setArrangement(inputs[0]);
    outputBus->setArrangement(outputs[0]);
    return kResultOk;
}

tresult PLUGIN_API PGClipGainProcessor::canProcessSampleSize(
    int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64
        ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API PGClipGainProcessor::process(ProcessData& data) {
    if (data.numSamples < 0) {
        return kResultFalse;
    }
    if (data.numSamples == 0 || data.numInputs == 0 || data.numOutputs == 0) {
        return kResultOk;
    }
    if (data.inputs == nullptr || data.outputs == nullptr) {
        return kResultFalse;
    }

    const auto& input = data.inputs[0];
    auto& output = data.outputs[0];

    if (input.numChannels <= 0 || input.numChannels != output.numChannels) {
        return kResultFalse;
    }

    output.silenceFlags = input.silenceFlags;

    if (data.symbolicSampleSize == kSample32) {
        copyAudioBus<float>(input, output, input.numChannels, data.numSamples);
    } else if (data.symbolicSampleSize == kSample64) {
        copyAudioBus<double>(input, output, input.numChannels, data.numSamples);
    } else {
        return kResultFalse;
    }

    return kResultOk;
}

tresult PLUGIN_API PGClipGainProcessor::setState(IBStream* state) {
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

tresult PLUGIN_API PGClipGainProcessor::getState(IBStream* state) {
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
