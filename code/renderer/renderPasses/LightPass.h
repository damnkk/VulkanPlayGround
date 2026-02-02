#ifndef LIGHTPASS_H
#define LIGHTPASS_H
#include "RDG/RDG.h"
#include "RenderPass.h"
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
    DeferRenderer* _ownedRender      = nullptr;
    RenderProgram* _lightPassProgram = nullptr;
};

} // namespace Play

#endif // LIGHTPASS_H