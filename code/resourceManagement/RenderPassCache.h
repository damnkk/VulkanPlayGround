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
    RenderPassCache(PlayElement* element);
    ~RenderPassCache();

private:
    PlayElement* _element = nullptr;
};

} // namespace Play
#endif // RENDERPASSCACHE_H