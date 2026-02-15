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
class PassNode;
// using TextureHandle      = RDGHandle<RDGTexture, uint32_t>;
// using BufferHandle       = RDGHandle<RDGBuffer, uint32_t>;
// using TextureHandleArray = std::vector<TextureHandle>;
// using BufferHandleArray  = std::vector<BufferHandle>;

class RDGPass;
class RenderDependencyGraph;
struct ProducerInfo
{
    PassNode*      lastProducer         = nullptr; // sync for write after read
    PassNode*      lastReadOnlyAccesser = nullptr; // sync for read after write
    VkAccessFlags2 accessMask           = 0;
};

struct RDGResourceAccessInfo
{
};

class TextureAccessInfo : public RDGResourceAccessInfo
{
public:
    TextureAccessInfo()                    = default;
    VkAccessFlags2        accessMask       = 0;
    VkImageLayout         layout           = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags2 stageMask        = 0;
    uint32_t              queueFamilyIndex = 0;

    uint32_t         set             = 0;
    uint32_t         binding         = 0;
    uint32_t         descriptorIndex = 0;
    VkDescriptorType descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    // ------ attachment info ------
    bool     isAttachment        = false;
    bool     isResolveAttachment = false;
    uint32_t attachSlotIdx       = ~0U;

    VkAttachmentLoadOp  loadOp;
    VkAttachmentStoreOp storeOp;

    VkImageLayout attachFinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool          operator==(TextureAccessInfo& candidate)
    {
        return accessMask == candidate.accessMask && layout == candidate.layout && stageMask == candidate.stageMask;
    }
};

class BufferAccessInfo : public RDGResourceAccessInfo
{
public:
    BufferAccessInfo()                     = default;
    VkAccessFlags2        accessMask       = 0;
    VkDescriptorType      descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkPipelineStageFlags2 stageMask        = 0;
    uint32_t              queueFamilyIndex = 0;
    VkDeviceSize          offset           = 0;
    VkDeviceSize          size             = VK_WHOLE_SIZE;
    uint32_t              set              = 0;
    uint32_t              binding          = 0;
    uint32_t              descriptorIndex  = 0;
    bool                  operator==(BufferAccessInfo& candidate)
    {
        return accessMask == candidate.accessMask && stageMask == candidate.stageMask && offset == candidate.offset && size == candidate.size &&
               queueFamilyIndex == candidate.queueFamilyIndex;
    }
};

using TextureSubresourceAccessInfo = std::vector<TextureAccessInfo>;

class RDGTexture
{
public:
    RDGTexture(std::string name) : _name(std::move(name)) {}
    ~RDGTexture();
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
        VkFormat              _format      = VK_FORMAT_UNDEFINED;
        VkImageType           _type        = VK_IMAGE_TYPE_2D;
        VkExtent3D            _extent      = {1, 1, 1};
        VkImageUsageFlags     _usageFlags  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageAspectFlags    _aspectFlags = VK_IMAGE_ASPECT_NONE;
        VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t              _mipmapLevel = 1;
        uint32_t              _layerCount  = 1;
        std::string           _debugName;
    } _info;

    TextureAccessInfo* getFinalAccessInfo()
    {
        return _externalState;
    }

private:
    friend class RDGBuilder;
    friend class RDGTextureBuilder;
    uint32_t           _refCount = 0;
    Texture*           _rhi      = nullptr;
    ProducerInfo       _producerInfo;
    TextureAccessInfo* _externalState = nullptr;
    // latest access info for each sub resource
    TextureSubresourceAccessInfo _subResourceAccessInfos;
    std::string                  _name;
};
using RDGTextureRef = RDGTexture*;

class RDGBuffer
{
public:
    RDGBuffer(std::string name) : _name(std::move(name)) {}
    ~RDGBuffer();
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
        VkBufferUsageFlags _usageFlags = VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
        VkDeviceSize       _size       = 8;
        VkDeviceSize       _range      = VK_WHOLE_SIZE;
        enum class MemoryLocation : uint8_t
        {
            eDeviceLocal,
            eHostVisible
        } _location = MemoryLocation::eDeviceLocal;
        std::string _debugName;
    } _info;

private:
    friend class RDGBuilder;
    friend class RDGBufferBuilder;
    uint32_t         _refCount = 0;
    Buffer*          _rhi;
    ProducerInfo     _producerInfo;
    BufferAccessInfo _latestAccessInfo;
    std::string      _name;
};
using RDGBufferRef = RDGBuffer*;
class BindlessTextureNode
{
};

class BindlessBufferNode
{
};

} // namespace Play::RDG

#endif // RDG_RESOURCES_H