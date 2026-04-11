#ifndef VOLUMESKYPASS_H
#define VOLUMESKYPASS_H
#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory.h>
#include "PlayProgram.h"
#include "controlComponent/controlComponent.h"

namespace Play
{

class DeferRenderer;
class VolumeSkyPass : public BasePass
{
public:
    VolumeSkyPass() = default;
    VolumeSkyPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~VolumeSkyPass() override;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;
    virtual void onGUI() override;

private:
    struct AtmosControler : public ControlComponent<AtmosParameter>
    {
    public:
        virtual void onGUI() override;
    } _skyAtmosControler;
    RefPtr<ComputeProgram> _transmittanceLutProgram;
    RefPtr<RenderProgram> _skyBoxProgram;
    RefPtr<RenderProgram> _atmosphereProgram;
    RefPtr<RenderProgram> _volumetricCloudProgram;
    DeferRenderer*        _ownedRender = nullptr;

    // 常驻资源
    RefPtr<Texture> _transmittanceLut;
};

} // namespace Play

#endif // VOLUMESKYPASS_H
