#ifndef GAUSSIAN_SORT_PASS_H
#define GAUSSIAN_SORT_PASS_H
#include "renderpasses/RenderPass.h"
#include "RDG/RDG.h"
#include "vk_radix_sort.h"
#include "core/RefCounted.h"
#include <rttr/rttr_enable.h>
namespace Play
{
class GaussianRenderer;
class GaussianSortPass : public BasePass
{
public:
    GaussianSortPass(GaussianRenderer* renderer);
    ~GaussianSortPass();
    void RenderFrame();
    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

    RTTR_ENABLE(BasePass)

private:
    VrdxSorter                     _sorter = VK_NULL_HANDLE;
    VrdxSorterStorageRequirements  _sortRequirements;
    GaussianRenderer*              _ownedRenderer = nullptr;
    ComputePipelineStateInitializer _distancePipeline;
};

} // namespace Play

#endif // GAUSSIAN_SORT_PASS_H
