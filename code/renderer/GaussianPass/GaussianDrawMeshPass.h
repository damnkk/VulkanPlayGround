#ifndef GAUSSIAN_DRAW_MESH_PASS_H
#define GAUSSIAN_DRAW_MESH_PASS_H
#include "renderpasses/RenderPass.h"
#include "RDG/RDG.h"
#include "core/RefCounted.h"

namespace Play
{
class GaussianRenderer;
class RenderProgram;
class MeshRenderProgram;

class GaussianDrawMeshPass : public BasePass
{
public:
    GaussianDrawMeshPass(GaussianRenderer* renderer);
    ~GaussianDrawMeshPass();
    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    GaussianRenderer*          _ownedRenderer = nullptr;
    RefPtr<MeshRenderProgram>  _meshRenderProgram;
    RefPtr<RenderProgram>      _presentProgram;
};

} // namespace Play

#endif // GAUSSIAN_DRAW_MESH_PASS_H
