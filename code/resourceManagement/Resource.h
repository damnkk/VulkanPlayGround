#ifndef RESOURCE_H
#define RESOURCE_H
#include "stdint.h"
#include "vector"
#include "nvvk/resources.hpp"
#include "nvvk/descriptors.hpp"
#include "PlayAllocator.h"
#include "ShaderManager.hpp"
namespace Play
{
class PlayAllocator;
class Texture;
class Buffer;

class Texture : public nvvk::Image
{
public:
    static Texture* Create();
    static Texture* Create(std::string name);
    static Texture* Create(uint32_t width, uint32_t height, VkFormat format,
                           VkImageUsageFlags usage,
                           VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED, uint32_t mipLevels = 1,
                           VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    static Texture* Create(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                           VkImageUsageFlags usage, VkImageLayout initialLayout,
                           uint32_t mipLevels = 1);
    static Texture* Create(uint32_t size, VkFormat format, VkImageUsageFlags usage,
                           VkImageLayout initialLayout, uint32_t mipLevels = 1);
    static Texture* Create(const std::filesystem::path& imagePath,
                           VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           uint32_t      mipLevels   = 1);
    static void     Destroy(Texture* texture);
    struct TexMetaData
    {
        VkFormat              format = VK_FORMAT_UNDEFINED;
        VkImageType           type   = VK_IMAGE_TYPE_2D;
        VkExtent3D            extent;
        VkImageUsageFlags     usageFlags;
        VkImageAspectFlags    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t              mipmapLevel = 1;
        uint32_t              layerCount  = 1;
        std::string           debugName;
    };
    Texture(int poolID) : poolId(poolID) {};
    int Id()
    {
        return poolId;
    }
    VkFormat& Format()
    {
        return format;
    }
    VkImageType& Type()
    {
        return type;
    }
    VkExtent3D& Extent()
    {
        return extent;
    }
    VkSampleCountFlagBits& SampleCount()
    {
        return sampleCount;
    }
    std::string& DebugName()
    {
        return debugName;
    }
    uint32_t& MipLevel()
    {
        return mipLevels;
    }
    bool isDepth() const
    {
        return aspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    }
    VkImageLayout& Layout()
    {
        return descriptor.imageLayout;
    }
    VkImageUsageFlags& UsageFlags()
    {
        return usageFlags;
    }
    VkImageAspectFlags& AspectFlags()
    {
        return aspectFlags;
    }
    uint32_t& LayerCount()
    {
        return arrayLayers;
    }
    TexMetaData getMetaData()
    {
        return {format,      type,      extent,      usageFlags, aspectFlags,
                sampleCount, mipLevels, arrayLayers, debugName};
    }

    void setMetaData(TexMetaData& metadata)
    {
        format      = metadata.format;
        type        = metadata.type;
        extent      = metadata.extent;
        usageFlags  = metadata.usageFlags;
        aspectFlags = metadata.aspectFlags;
        sampleCount = metadata.sampleCount;
        mipLevels   = metadata.mipmapLevel;
        arrayLayers = metadata.layerCount;
        debugName   = metadata.debugName;
    }

    bool isValid() const
    {
        return image != VK_NULL_HANDLE;
    }

protected:
    friend class TexturePool;
    int                   poolId      = -1;
    VkImageType           type        = VK_IMAGE_TYPE_2D;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    std::string           debugName;
    VkImageAspectFlags    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageUsageFlags     usageFlags =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
};

class Buffer : public nvvk::Buffer
{
public:
    static Buffer* Create();
    static Buffer* Create(std::string name);
    static void    Destroy(Buffer* buffer);

    struct BufferMetaData
    {
        VkBufferUsageFlags    _usageFlags;
        VkDeviceSize          _size;
        VkDeviceSize          _range = VK_WHOLE_SIZE;
        VkMemoryPropertyFlags _property;
    };
    Buffer(int poolID) : poolId(poolID) {};
    int Id()
    {
        return poolId;
    }
    int                    poolId = -1;
    VkDescriptorBufferInfo descriptor;
    std::string&           DebugName()
    {
        return debugName;
    }
    VkBufferUsageFlags& UsageFlags()
    {
        return usageFlags;
    }
    VkDeviceSize& BufferSize()
    {
        return bufferSize;
    }
    VkDeviceSize& BufferRange()
    {
        return range;
    }
    VkMemoryPropertyFlags& BufferProperty()
    {
        return property;
    }

    BufferMetaData getMetaData()
    {
        return BufferMetaData{usageFlags, bufferSize, range, property};
    }

    void setMetaData(BufferMetaData& metadata)
    {
        usageFlags = metadata._usageFlags;
        size       = metadata._size;
        range      = metadata._range;
        property   = metadata._property;
    }

    bool isValid() const
    {
        return buffer != VK_NULL_HANDLE;
    }

protected:
    friend class BufferPool;
    VkBufferUsageFlags    usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkDeviceSize          range      = VK_WHOLE_SIZE;
    VkDeviceSize          size       = 0;
    std::string           debugName;
    VkMemoryPropertyFlags property;
};

class GraphicsPipelineState
{
public:
    VkSampleMask                           sampleMask{~0U};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        // pNext will be set to `rasterizationLineState` when used in GraphicsPipelineCreator
        .flags                   = 0,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp          = 0,
        .depthBiasSlopeFactor    = 0,
        .lineWidth               = 1,
    };
    VkPipelineRasterizationLineStateCreateInfo rasterizationLineState{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO,
        .lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_DEFAULT,
        .stippledLineEnable    = VK_FALSE,
        .lineStippleFactor     = 1,
        .lineStipplePattern    = 0xAA,
    };
    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
        .minSampleShading     = 0,
        .pSampleMask = 0, // do not use, implicitly set to &sampleMask in GraphicsPipelineCreator
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };
    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {},
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_CLEAR,
        .attachmentCount = 0, // do not use, implicitly set GraphicsPipelineCreator
        .pAttachments    = 0, // do not use, implicitly set GraphicsPipelineCreator
        .blendConstants  = {1.0f, 1.0f, 1.0f, 1.0f},
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType                         = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0, // do not use, implicitly set in GraphicsPipelineCreator
        .pVertexBindingDescriptions    = 0, // do not use, implicitly set in GraphicsPipelineCreator
        .vertexAttributeDescriptionCount =
            0,                             // do not use, implicitly set in GraphicsPipelineCreator
        .pVertexAttributeDescriptions = 0, // do not use, implicitly set in GraphicsPipelineCreator
    };
    VkPipelineTessellationStateCreateInfo tessellationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        // pNext will be set to `tessellationDomainOriginState` when used in GraphicsPipelineCreator
        .patchControlPoints = 4,
    };
    VkPipelineTessellationDomainOriginStateCreateInfo tessellationDomainOriginState{
        .sType        = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,
        .domainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT,
    };
    // by default we enable 1 color attachment with disabled blending
    std::vector<VkBool32>              colorBlendEnables{VK_FALSE};
    std::vector<VkColorComponentFlags> colorWriteMasks{
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT};
    std::vector<VkColorBlendEquationEXT>  colorBlendEquations{{
         .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
         .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
         .colorBlendOp        = VK_BLEND_OP_ADD,
         .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
         .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
         .alphaBlendOp        = VK_BLEND_OP_ADD,
    }};
    std::vector<nvvk::DescriptorBindings> descriptorBindings;
    ShaderModule                          vertexShader;
    ShaderModule                          fragmentShader;
};

class ComputePipelineState
{
public:
    std::vector<nvvk::DescriptorBindings> descriptorBindings;
    ShaderModule                          computeShader;
};
} // namespace Play
#endif // RESOURCE_H