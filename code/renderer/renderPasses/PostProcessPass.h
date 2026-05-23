#ifndef POSTPROCESSPASS_H
#define POSTPROCESSPASS_H
#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory>
#include <nvvk/graphics_pipeline.hpp>
#include <nvshaders_host/tonemapper.hpp>
#include <rttr/rttr_enable.h>
#include "controlComponent/controlComponent.h"
namespace Play
{
class ComputeProgram;
class DeferRenderer;
class PostProcessPass : public BasePass
{
public:
    PostProcessPass(DeferRenderer* ownedRender);
    ~PostProcessPass() override;

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

    RTTR_ENABLE(BasePass)

private:
    friend class DeferRenderer;
    RefPtr<ComputeProgram> _postProgram;
    DeferRenderer*         _ownedRender                 = nullptr;
    ToneMappingControlComponent _tonemapperControlComponent;
    nvshaders::Tonemapper       _tonemapper;
};

} // namespace Play

#endif // POSTPROCESSPASS_H
