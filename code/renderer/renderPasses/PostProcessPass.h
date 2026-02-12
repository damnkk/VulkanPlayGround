#ifndef POSTPROCESSPASS_H
#define POSTPROCESSPASS_H
#include "RenderPass.h"
#include <memory>
#include <nvvk/graphics_pipeline.hpp>
#include <nvshaders_host/tonemapper.hpp>
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

private:
    friend class DeferRenderer;
    ComputeProgram*       _postProgram;
    DeferRenderer*        _ownedRender = nullptr;
    nvshaders::Tonemapper _tonemapper;
};

} // namespace Play

#endif // POSTPROCESSPASS_H