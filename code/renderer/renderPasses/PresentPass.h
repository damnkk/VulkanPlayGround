#ifndef PLAY_PRESENTPASS_H
#define PLAY_PRESENTPASS_H

#include "RenderPass.h"
#include "PipelineCacheManager.h"
#include "core/RefCounted.h"
#include <memory>
#include <rttr/rttr_enable.h>
namespace Play
{
class Renderer;

class PresentPass : public BasePass
{
public:
    PresentPass(Renderer* renderer) : _renderer(renderer) {}
    static constexpr const char* PRESENT_TEXTURE_NAME = "presentTexture";

    ~PresentPass();

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

    RTTR_ENABLE(BasePass)

private:
    Renderer*                         _renderer = nullptr;
    GraphicsPipelineStateInitializer _presentPipeline;
};

} // namespace Play

#endif // PLAY_PRESENTPASS_H
