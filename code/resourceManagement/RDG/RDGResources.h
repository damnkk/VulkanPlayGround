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
    ResourceNode(size_t id, NodeType type) : Node(id, type)
    {
        m_priority = NodePriority::eSecondary;
    }
};

class TextureNode : public ResourceNode
{
public:
    TextureNode(size_t id, std::string name)
        : ResourceNode(id, NodeType::eTexture), _name(std::move(name))
    {
    }
    Texture* getRHI()
    {
        return _rhi;
    }

    void setRHI(Texture* rhi)
    {
        _rhi = rhi;
    }

    inline std::string name() const
    {
        return _name;
    }

    struct TextureDesc
    {
        VkFormat          _format = VK_FORMAT_UNDEFINED;
        VkImageType       _type   = VK_IMAGE_TYPE_2D;
        VkExtent3D        _extent = {1, 1, 1};
        VkImageUsageFlags _usageFlags =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageAspectFlags    _aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t              _mipmapLevel = 1;
        uint32_t              _layerCount  = 1;
        std::string           _debugName;
    } _info;

private:
    Texture*    _rhi;
    std::string _name;
};
using TextureNodeRef = TextureNode*;
class BufferNode : public ResourceNode
{
public:
    BufferNode(size_t i, std::string name)
        : ResourceNode(i, NodeType::eBuffer), _name(std::move(name))
    {
    }
    Buffer* getRHI()
    {
        return _rhi;
    }

    void setRHI(Buffer* rhi)
    {
        _rhi = rhi;
    }

    inline std::string name() const
    {
        return _name;
    }

    struct BufferDesc
    {
        VkBufferUsageFlags _usageFlags =
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
        VkDeviceSize _size  = 8;
        VkDeviceSize _range = VK_WHOLE_SIZE;
        enum class MemoryLocation : uint8_t
        {
            eDeviceLocal,
            eHostVisible
        } _location = MemoryLocation::eDeviceLocal;
        std::string _debugName;
    } _info;

private:
    Buffer*     _rhi;
    std::string _name;
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