#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "resourceManagement/vulkan/resources/Resource.h"
#include "RDGHandle.h"
#include "core/RefCounted.h"
#include "nvvk/pipeline.hpp"
#include "nvvk/graphics_pipeline.hpp"
#include "nvvk/compute_pipeline.hpp"
#include "nvvk/shaders.hpp"
#include "resourceManagement/vulkan/pipeline/ShaderManager.hpp"
#include <optional>
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

    VkAttachmentLoadOp  loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

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

class RDGTexture
{
public:
    RDGTexture(std::string name) : _name(std::move(name)) {}
    ~RDGTexture();
    Texture* getRHI()
    {
        return _rhi.get();
    }

    void import(Texture* rhi)
    {
        _external = true;
        _rhi      = rhi;
        if (rhi)
        {
            _info._aspectFlags = rhi->AspectFlags();
            _info._format      = rhi->Format();
            _info._extent      = rhi->Extent();
            _info._type        = rhi->Type();
            _info._usageFlags  = rhi->UsageFlags();
            _info._sampleCount = rhi->SampleCount();
            _info._mipmapLevel = rhi->MipLevel();
            _info._layerCount  = rhi->LayerCount();
        }
    }

    const std::string& name() const
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
    } _info;

private:
    friend class RDGBuilder;
    friend class RDGTextureBuilder;
    RefPtr<Texture> _rhi;
    std::string     _name;
    bool            _external     = false;
    uint32_t        _firstUsePass = ~0U;
    uint32_t        _lastUsePass  = ~0U;
};
using RDGTextureRef = RDGTexture*;

class RDGBuffer
{
public:
    RDGBuffer(std::string name) : _name(std::move(name)) {}
    ~RDGBuffer();
    Buffer* getRHI()
    {
        return _rhi.get();
    }

    void import(Buffer* rhi)
    {
        _rhi = rhi;
    }

    const std::string& name() const
    {
        return _name;
    }

    struct BufferDesc
    {
        VkBufferUsageFlags2 _usageFlags = VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
        VkDeviceSize        _size       = 8;
        VkDeviceSize        _range      = VK_WHOLE_SIZE;
        enum class MemoryLocation : uint8_t
        {
            eDeviceLocal,
            eHostVisible
        } _location = MemoryLocation::eDeviceLocal;
    } _info;

private:
    friend class RDGBuilder;
    friend class RDGBufferBuilder;
    RefPtr<Buffer> _rhi;
    std::string    _name;
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
