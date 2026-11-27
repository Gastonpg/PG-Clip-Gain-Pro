#pragma once

#include "pluginterfaces/base/funknown.h"

namespace Steinberg {
namespace Vst {

static const FUID PGClipGainProcessorUID(
    0x7B5D2A91, 0xC4E843F2, 0x9D6A17B0, 0xE3F5C821);

static const FUID PGClipGainControllerUID(
    0x2C8F4E63, 0xA1D749B5, 0xB30E6F92, 0xD7A4185C);

static constexpr uint32 kCaptureEnableParameterId = 1000U;

} // namespace Vst
} // namespace Steinberg
