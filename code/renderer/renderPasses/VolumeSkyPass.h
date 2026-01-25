#ifndef VOLUMESKYPASS_H
#define VOLUMESKYPASS_H
#include "RenderPass.h"
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
    virtual ~VolumeSkyPass() = default;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    RenderProgram* _skyBoxProgram;
    RenderProgram* _atmosphereProgram;
    RenderProgram* _volumetricCloudProgram;
    DeferRenderer* _ownedRender = nullptr;
};

} // namespace Play

#endif // VOLUMESKYPASS_H