#ifndef GAUSSIAN_SORT_PASS_H
#define GAUSSIAN_SORT_PASS_H
#include "renderpasses/RenderPass.h"
#include "RDG/RDG.h"
namespace Play
{
class ComputeProgram;
class GaussianRenderer;
class GaussianSortPass : public BasePass
{
public:
    GaussianSortPass(GaussianRenderer* renderer);
    ~GaussianSortPass();
    void RenderFrame();
    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    GaussianRenderer* _ownedRenderer;
    ComputeProgram*   _distanceProgram;
};

} // namespace Play

#endif // GAUSSIAN_SORT_PASS_H