#ifndef LIGHTPASS_H
#define LIGHTPASS_H
#include "RDG/RDG.h"
#include "RenderPass.h"
#include "core/RefCounted.h"
namespace Play
{
class DeferRenderer;
class RenderProgram;
class LightPass : public BasePass
{
public:
    LightPass() = default;
    LightPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~LightPass() override;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    DeferRenderer*        _ownedRender = nullptr;
    RefPtr<RenderProgram> _lightPassProgram;
};

} // namespace Play

#endif // LIGHTPASS_H