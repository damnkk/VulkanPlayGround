#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "Resource.h"
#include "RDGHandle.h"
#include "nvvk/pipeline.hpp"
#include "nvvk/graphics_pipeline.hpp"
#include "nvvk/compute_pipeline.hpp"
#include "nvvk/shaders.hpp"
#include "ShaderManager.hpp"
#include <optional>
#include "BaseDag.h"
namespace Play::RDG
{

class RDGTexture;
class RDGBuffer;
using TextureHandle      = RDGHandle<RDGTexture, uint32_t>;
using BufferHandle       = RDGHandle<RDGBuffer, uint32_t>;
using TextureHandleArray = std::vector<TextureHandle>;
using BufferHandleArray  = std::vector<BufferHandle>;

class RDGPass;
class RDGTexturePool;
class RenderDependencyGraph;

class ResourceNode : public Node
{
public:
    ResourceNode(size_t id) : Node(id) {}
};

class TextureNode : public ResourceNode
{
public:
    TextureNode(size_t id, Texture* rhiHandle) : ResourceNode(id), _rhi(rhiHandle) {}
    Texture* getRHI()
    {
        return _rhi;
    }

private:
    Texture* _rhi;
};
using TextureNodeRef = TextureNode*;
class BufferNode : public ResourceNode
{
public:
    BufferNode(size_t id, Buffer* rhiHandle) : ResourceNode(id), _rhi(rhiHandle) {}
    Buffer* getRHI()
    {
        return _rhi;
    }

private:
    Buffer* _rhi;
};
using BufferNodeRef = BufferNode*;
class BindlessTextureNode : public ResourceNode
{
};

class BindlessBufferNode : public ResourceNode
{
};

class TextureEdge : public Edge
{
public:
    TextureEdge(Node* from, Node* to, EdgeType type = EdgeType::eTexture) : Edge(from, to, type) {}
    VkAccessFlags2        accessMask;
    VkImageLayout         layout;
    VkPipelineStageFlags2 stageMask;
    uint32_t              queueFamilyIndex;

    uint32_t set             = 0;
    uint32_t binding         = 0;
    uint32_t descriptorIndex = 0;
};

class AttachmentEdge : public Edge
{
public:
    AttachmentEdge(Node* from, Node* to, EdgeType type = EdgeType::eRenderAttachment)
        : Edge(from, to, type)
    {
    }
    uint32_t slotIdx;
    union attachOperation
    {
        VkAttachmentLoadOp  loadOp;
        VkAttachmentStoreOp storeOp;
    };
    attachOperation attachmentOp;
    VkImageLayout   layout;
};

class BufferEdge : public Edge
{
public:
    BufferEdge(Node* from, Node* to, EdgeType type = EdgeType::eBuffer) : Edge(from, to, type) {}
    VkAccessFlags2        accessMask;
    VkPipelineStageFlags2 stageMask;
    uint32_t              queueFamilyIndex;
    VkDeviceSize          offset          = 0;
    VkDeviceSize          size            = VK_WHOLE_SIZE;
    uint32_t              set             = 0;
    uint32_t              binding         = 0;
    uint32_t              descriptorIndex = 0;
};

} // namespace Play::RDG

#endif // RDG_RESOURCES_H