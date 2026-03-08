#ifndef GAUSSIAN_SORT_PASS_H
#define GAUSSIAN_SORT_PASS_H
#include "renderpasses/RenderPass.h"
#include "RDG/RDG.h"
#include "vk_radix_sort.h"
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
    VrdxSorter                    _sorter = VK_NULL_HANDLE;
    VrdxSorterStorageRequirements _sortRequirements;
    GaussianRenderer*             _ownedRenderer;
    ComputeProgram*               _distanceProgram;
};

} // namespace Play

#endif // GAUSSIAN_SORT_PASS_H