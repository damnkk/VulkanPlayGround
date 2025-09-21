/*
For increase multi-pass rendering development efficiency,and performance,I designed a simple render
dependency graph. for the first development stage, the graph can not be modified at runtime and only
support single threaded execution(or not). Usage of the graph shoule as simple as possible,only
shader needed and resource specified in the pass.And dependency between passes should be specified
by the resource usage.The design must considering RT resuse in the future,so, a texture handle wrap
is needed. we use resource handle as resource itself,to confirm the dependency, and specify the real
GPU resource when RDG compile.
 */
#ifndef RDG_H
#define RDG_H
#include "string"
#include "unordered_set"
#include "functional"
#include "array"
#include "list"
#include "RDGPreDefine.h"
#include "PlayApp.h"
#include "RDGResources.h"
#include "RDGPasses.hpp"
namespace Play::RDG
{
// we can reuse some texture here, to save some gpu memory
class RDGTextureCache
{
public:
    RDGTextureCache()  = default;
    ~RDGTextureCache() = default;
    void        regist(Texture* texture);
    RDGTexture* request(Texture* texture);

private:
    std::unordered_map<Texture::TexMetaData, nvvk::Image> _textureMap;
};

class RDGBuilder;
class RDGTextureBuilder
{
public:
    RDGTextureBuilder(RDGBuilder* builder, TextureNodeRef node)
        : _builder(builder), _textureNode(node)
    {
    }
    RDGTextureBuilder& Format(VkFormat format);
    RDGTextureBuilder& Type(VkImageType type);
    RDGTextureBuilder& Extent(VkExtent3D extent);
    RDGTextureBuilder& UsageFlags(VkImageUsageFlags usageFlags);
    RDGTextureBuilder& AspectFlags(VkImageAspectFlags aspectFlags);
    RDGTextureBuilder& SampleCount(VkSampleCountFlagBits sampleCount);
    RDGTextureBuilder& MipmapLevel(uint32_t mipmapLevel);
    RDGTextureBuilder& LayerCount(uint32_t layerCount);
    TextureNodeRef     finish();

private:
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
    } _desc;
    RDGBuilder*    _builder     = nullptr;
    TextureNodeRef _textureNode = nullptr;
};
class RDGBufferBuilder
{
public:
    RDGBufferBuilder(RDGBuilder* builder, BufferNodeRef node) : _builder(builder), _bufferNode(node)
    {
    }
    RDGBufferBuilder& Size(VkDeviceSize size);
    RDGBufferBuilder& Range(VkDeviceSize range);
    RDGBufferBuilder& UsageFlags(VkBufferUsageFlags usageFlags);
    RDGBufferBuilder& Location(bool isDeviceLocal);
    BufferNodeRef     finish();

private:
    struct BufferDesc
    {
        VkBufferUsageFlags _usageFlags =
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
        VkDeviceSize _size  = 1;
        VkDeviceSize _range = VK_WHOLE_SIZE;
        enum class MemoryLocation : uint8_t
        {
            eDeviceLocal,
            eHostVisible
        } _location = MemoryLocation::eDeviceLocal;
        std::string _debugName;
    } _desc;
    RDGBuilder*   _builder    = nullptr;
    BufferNodeRef _bufferNode = nullptr;
};

class BlackBoard
{
public:
    BlackBoard()  = default;
    ~BlackBoard() = default;
    void           registTexture(std::string name, TextureNodeRef texture);
    void           registBuffer(std::string name, BufferNodeRef buffer);
    void           registPass(std::string name, PassNode* pass);
    TextureNodeRef getTexture(std::string name);
    BufferNodeRef  getBuffer(std::string name);
    PassNode*      getPass(std::string name);

private:
    std::unordered_map<std::string, TextureNodeRef> _textureMap;
    std::unordered_map<std::string, BufferNodeRef>  _bufferMap;
    std::unordered_map<std::string, PassNode*>      _passMap;
};

class RDGBuilder
{
public:
    RDGBuilder(PlayElement* element);
    ~RDGBuilder();
    RenderPassBuilder  createRenderPass(std::string name);
    ComputePassBuilder createComputePass(std::string name);
    RTPassBuilder      createRTPass(std::string name);

    RDGTextureBuilder createTexture(std::string name);
    RDGBufferBuilder  createBuffer(std::string name);

    TextureNodeRef getTexture(std::string name);
    BufferNodeRef  getBuffer(std::string name);

    void execute(RenderPassNode* pass);
    void execute(ComputePassNode* pass);
    void execute(RTPassNode* pass);

    Dag* getDag()
    {
        return _dag;
    }

private:
    friend class RenderPassBuilder;
    friend class ComputePassBuilder;
    friend class RTPassBuilder;
    PlayElement*                                 _element = nullptr;
    Dag*                                         _dag     = nullptr;
    std::vector<PassNode*>                       _passes;
    BlackBoard                                   _blackBoard;
    std::unordered_map<std::string, RDGTexture*> _textureMap;
    std::unordered_map<std::string, RDGBuffer*>  _bufferMap;
};

} // namespace Play::RDG

#endif // RDG_H