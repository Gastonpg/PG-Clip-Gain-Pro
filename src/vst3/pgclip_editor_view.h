#pragma once

#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace Steinberg {
namespace Vst {

class PGClipGainEditorView final : public CPluginView {
public:
    explicit PGClipGainEditorView(IEditController* controller);
    ~PGClipGainEditorView() SMTG_OVERRIDE;

    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) SMTG_OVERRIDE;
    tresult PLUGIN_API attached(void* parent, FIDString type) SMTG_OVERRIDE;
    tresult PLUGIN_API removed() SMTG_OVERRIDE;

    void attachedToParent() SMTG_OVERRIDE;
    void removedFromParent() SMTG_OVERRIDE;

private:
    IEditController* controller_{nullptr};
    void* editorWindow_{nullptr};
};

} // namespace Vst
} // namespace Steinberg
