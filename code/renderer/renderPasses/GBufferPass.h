#ifndef GBUFFERPASS_H
#define GBUFFERPASS_H
#include <rttr/rttr_enable.h>
#include "RenderPass.h"
#include "GBufferConfig.h"
#include "resourceManagement/vulkan/pipeline/PipelineCacheManager.h"

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

    RTTR_ENABLE(BasePass)

private:
    DeferRenderer*                   _ownedRender = nullptr;
    GraphicsPipelineStateInitializer _gbufferPipeline;
};
} // namespace Play

#endif // GBUFFERPASS_H
