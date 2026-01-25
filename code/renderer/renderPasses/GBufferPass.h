#ifndef GBUFFERPASS_H
#define GBUFFERPASS_H
#include "RenderPass.h"
#include "GBufferConfig.h"
namespace Play
{
class DeferRenderer;

class GBufferPass : public BasePass
{
public:
    GBufferPass() = default;
    GBufferPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~GBufferPass() = default;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    DeferRenderer* _ownedRender = nullptr;
};
} // namespace Play

#endif // GBUFFERPASS_H