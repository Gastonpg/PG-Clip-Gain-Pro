#include "pgclip_cids.h"
#include "pgclip_controller.h"
#include "pgclip_processor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "PG Clip Gain Pro"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(
    "Gaston Puig",
    "https://github.com/Gastonpg/PG-Clip-Gain-Pro",
    ""
)

DEF_CLASS2(
    INLINE_UID_FROM_FUID(PGClipGainProcessorUID),
    PClassInfo::kManyInstances,
    kVstAudioEffectClass,
    stringPluginName,
    Vst::kDistributable,
    "Fx|Tools",
    FULL_VERSION_STR,
    kVstVersionString,
    PGClipGainProcessor::createInstance
)

DEF_CLASS2(
    INLINE_UID_FROM_FUID(PGClipGainControllerUID),
    PClassInfo::kManyInstances,
    kVstComponentControllerClass,
    stringPluginName " Controller",
    0,
    "",
    FULL_VERSION_STR,
    kVstVersionString,
    PGClipGainController::createInstance
)

END_FACTORY
