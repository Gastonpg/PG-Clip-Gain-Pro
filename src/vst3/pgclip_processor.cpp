#include "pgclip_processor.h"
#include "pgclip_cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/fstrdefs.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace Steinberg {
namespace Vst {
namespace {

constexpr int32 kStateMagic = 0x50474350; // "PGCP"
constexpr int32 kStateVersion = 1;

template <typename Sample>
Sample* outputChannel(AudioBusBuffers& bus, int32 channel) noexcept {
    if constexpr (std::is_same_v<Sample, float>) {
        return bus.channelBuffers32 ? bus.channelBuffers32[channel] : nullptr;
    } else {
        return bus.channelBuffers64 ? bus.channelBuffers64[channel] : nullptr;
    }
}

template <typename Sample>
const Sample* inputChannel(const AudioBusBuffers& bus, int32 channel) noexcept {
    if constexpr (std::is_same_v<Sample, float>) {
        return bus.channelBuffers32 ? bus.channelBuffers32[channel] : nullptr;
    } else {
        return bus.channelBuffers64 ? bus.channelBuffers64[channel] : nullptr;
    }
}

template <typename Sample>
bool copyAudioBus(const AudioBusBuffers& input,
                  AudioBusBuffers& output,
                  int32 channelCount,
                  int32 sampleCount) noexcept {
    const auto bytes = static_cast<std::size_t>(sampleCount) * sizeof(Sample);

    for (int32 channel = 0; channel < channelCount; ++channel) {
        auto* destination = outputChannel<Sample>(output, channel);
        const auto* source = inputChannel<Sample>(input, channel);

        if (destination == nullptr) {
            return false;
        }

        const bool channelIsSilent =
            (input.silenceFlags & (static_cast<uint64>(1) << channel)) != 0U;

        if (channelIsSilent || source == nullptr) {
            std::memset(destination, 0, bytes);
            if (source == nullptr) {
                output.silenceFlags |=
                    (static_cast<uint64>(1) << channel);
            }
        } else if (destination != source) {
            std::memcpy(destination, source, bytes);
        }
    }

    return true;
}

void handleCaptureParameter(ProcessData& data,
                            pgclip::Vst3CaptureRuntime& runtime,
                            std::uint32_t sampleRate,
                            std::uint32_t channels) noexcept {
    if (data.inputParameterChanges == nullptr) {
        return;
    }

    const int32 queueCount = data.inputParameterChanges->getParameterCount();
    for (int32 queueIndex = 0; queueIndex < queueCount; ++queueIndex) {
        auto* queue = data.inputParameterChanges->getParameterData(queueIndex);
        if (queue == nullptr ||
            queue->getParameterId() != kCaptureEnableParameterId) {
            continue;
        }

        const int32 pointCount = queue->getPointCount();
        for (int32 pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
            int32 sampleOffset = 0;
            ParamValue value = 0.0;
            if (queue->getPoint(pointIndex, sampleOffset, value) != kResultOk) {
                continue;
            }
            (void)sampleOffset;
            if (value >= 0.5) {
                runtime.requestArm(sampleRate, channels);
            } else {
                runtime.requestStop();
            }
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
    if (addAudioInput(STR16("Audio In"), SpeakerArr::kStereo) == nullptr ||
        addAudioOutput(STR16("Audio Out"), SpeakerArr::kStereo) == nullptr) {
        return kResultFalse;
    }

    return captureRuntime_.start() ? kResultOk : kResultFalse;
}

tresult PLUGIN_API PGClipGainProcessor::terminate() {
    captureRuntime_.shutdown();
    return AudioEffect::terminate();
}

tresult PLUGIN_API PGClipGainProcessor::setupProcessing(ProcessSetup& setup) {
    const tresult result = AudioEffect::setupProcessing(setup);
    if (result == kResultOk && setup.sampleRate > 0.0) {
        sampleRate_ = static_cast<std::uint32_t>(setup.sampleRate);
    }
    return result;
}

tresult PLUGIN_API PGClipGainProcessor::setProcessing(TBool state) {
    if (!state) {
        captureRuntime_.requestStop();
    }
    return AudioEffect::setProcessing(state);
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

    handleCaptureParameter(
        data, captureRuntime_, sampleRate_,
        static_cast<std::uint32_t>(input.numChannels));

    output.silenceFlags = input.silenceFlags;

    bool copied = false;
    if (data.symbolicSampleSize == kSample32) {
        copied = copyAudioBus<float>(
            input, output, input.numChannels, data.numSamples);
    } else if (data.symbolicSampleSize == kSample64) {
        copied = copyAudioBus<double>(
            input, output, input.numChannels, data.numSamples);
    } else {
        return kResultFalse;
    }

    if (!copied) {
        return kResultFalse;
    }

    if (captureRuntime_.state() == pgclip::CaptureSessionState::Armed) {
        if (data.processContext == nullptr) {
            captureRuntime_.requestStop();
        } else {
            const auto flags = data.processContext->state;
            const auto format = data.symbolicSampleSize == kSample32
                ? pgclip::AudioSampleFormat::Float32
                : pgclip::AudioSampleFormat::Float64;

            const void* channelPointers[2]{};
            for (int32 channel = 0; channel < input.numChannels; ++channel) {
                if (data.symbolicSampleSize == kSample32) {
                    channelPointers[channel] = input.channelBuffers32
                        ? static_cast<const void*>(input.channelBuffers32[channel])
                        : nullptr;
                } else {
                    channelPointers[channel] = input.channelBuffers64
                        ? static_cast<const void*>(input.channelBuffers64[channel])
                        : nullptr;
                }
            }

            captureRuntime_.captureBlock(
                channelPointers,
                static_cast<std::uint32_t>(input.numChannels),
                static_cast<std::uint32_t>(data.numSamples),
                data.processContext->projectTimeSamples,
                (flags & ProcessContext::kPlaying) != 0,
                (flags & ProcessContext::kCycleActive) != 0,
                format,
                input.silenceFlags);
        }
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
