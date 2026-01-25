#ifndef POSTPROCESSPASS_H
#define POSTPROCESSPASS_H
#include "RenderPass.h"
#include <memory>
#include <nvvk/graphics_pipeline.hpp>
namespace Play
{
class RenderProgram;
class DeferRenderer;
class PostProcessPass : public BasePass
{
public:
    PostProcessPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    ~PostProcessPass() override;

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    friend class DeferRenderer;
    RenderProgram* _postProgram;
    DeferRenderer* _ownedRender = nullptr;
};

} // namespace Play

#endif // POSTPROCESSPASS_H