#ifndef VOLUMESKYPASS_H
#define VOLUMESKYPASS_H
#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory.h>
#include "PlayProgram.h"

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

private:
    RefPtr<RenderProgram> _skyBoxProgram;
    RefPtr<RenderProgram> _atmosphereProgram;
    RefPtr<RenderProgram> _volumetricCloudProgram;
    DeferRenderer*        _ownedRender = nullptr;
};

} // namespace Play

#endif // VOLUMESKYPASS_H