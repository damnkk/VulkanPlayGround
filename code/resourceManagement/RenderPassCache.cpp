#include <RenderPassCache.h>
namespace Play
{

RenderPassCache::RenderPassCache(PlayElement* element)
{
    _element = element;

    // vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
}
RenderPassCache::~RenderPassCache() {}
} // namespace Play