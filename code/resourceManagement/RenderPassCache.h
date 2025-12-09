#ifndef RENDERPASSCACHE_H
#define RENDERPASSCACHE_H
#include <nvvk/gbuffers.hpp>

namespace Play
{
class PlayElement;

struct RenderPassKey
{
    std::vector<VkAttachmentDescription2> colorAttachments;
    std::vector<VkAttachmentDescription2> depthStencilAttachments;
};

class RenderPassCache
{
public:
    RenderPassCache();
    ~RenderPassCache();

private:
};

} // namespace Play
#endif // RENDERPASSCACHE_H