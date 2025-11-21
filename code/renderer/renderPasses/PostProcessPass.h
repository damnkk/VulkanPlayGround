#ifndef POSTPROCESSPASS_H
#define POSTPROCESSPASS_H
#include "RenderPass.h"
#include <memory>
#include <nvvk/graphics_pipeline.hpp>
namespace Play
{
class RenderProgram;
class PlayElement;
class PostProcessPass : public RenderPass
{
public:
    PostProcessPass(PlayElement* element) : _element(element) {}
    ~PostProcessPass() override;

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    std::unique_ptr<RenderProgram> _postProgram;
    nvvk::GraphicsPipelineState    _postPipelineState;
    PlayElement*                   _element = nullptr;
};

} // namespace Play

#endif // POSTPROCESSPASS_H