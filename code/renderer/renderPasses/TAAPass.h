#ifndef TAAPASS_H
#define TAAPASS_H

#include "RenderPass.h"
#include "core/RefCounted.h"
#include "resourceManagement/vulkan/pipeline/PipelineCacheManager.h"
#include <rttr/rttr_enable.h>

namespace Play
{
class DeferRenderer;

// TAA compresses HDR samples during resolve and keeps history in that space.
// A second dispatch updates history and writes inverse-mapped HDR back into
// scene color, which PostProcessPass then consumes.
class TAAPass : public BasePass
{
public:
    explicit TAAPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    ~TAAPass() override;

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

    static constexpr const char* INPUT_TEXTURE_NAME  = "LightPassOutput";
    static constexpr const char* OUTPUT_TEXTURE_NAME = INPUT_TEXTURE_NAME;

private:
    void ensureHistoryTexture();

    DeferRenderer*                  _ownedRender = nullptr;
    ComputePipelineStateInitializer _resolvePipeline;
    ComputePipelineStateInitializer _inverseTonemapPipeline;

    RefPtr<Texture> _historyTexture;
    bool            _historyValid = false;
};
} // namespace Play

#endif // TAAPASS_H
