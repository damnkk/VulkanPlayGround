#ifndef GAUSSIAN_DRAW_MESH_PASS_H
#define GAUSSIAN_DRAW_MESH_PASS_H
#include "renderpasses/RenderPass.h"
#include "RDG/RDG.h"
#include "core/RefCounted.h"
#include <rttr/rttr_enable.h>

namespace Play
{
class GaussianRenderer;

class GaussianDrawMeshPass : public BasePass
{
public:
    GaussianDrawMeshPass(GaussianRenderer* renderer);
    ~GaussianDrawMeshPass();
    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

    RTTR_ENABLE(BasePass)

private:
    GaussianRenderer*              _ownedRenderer = nullptr;
    GraphicsPipelineStateInitializer _meshRenderPipeline;
    GraphicsPipelineStateInitializer _presentPipeline;
};

} // namespace Play

#endif // GAUSSIAN_DRAW_MESH_PASS_H
