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
    std::unordered_map<TextureNode::TextureDesc, std::list<RDGTexture*>> _textureMap;
};

class RDGBuilder;
class RDGTextureBuilder
{
public:
    RDGTextureBuilder(RDGBuilder* builder, TextureNodeRef node)
        : _builder(builder), _textureNode(node)
    {
    }
    RDGTextureBuilder& Import(Texture* texture, VkAccessFlags2 accessMask, VkImageLayout layout,
                              VkPipelineStageFlags2 stageMask, uint32_t queueFamilyIndex);
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
    void           compile();
    void           execute();

    Dag* getDag()
    {
        return _dag;
    }

private:
    friend class RDGTextureBuilder;
    friend class RDGBufferBuilder;
    InputPassNodeRef createInputPass(std::string name);
    void             execute(RenderPassNode* pass);
    void             execute(ComputePassNode* pass);
    void             execute(RTPassNode* pass);
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

struct RenderContext
{
    PlayElement*                _element;
    PlayElement::PlayFrameData* _frameData;
    uint32_t                    _frameInFlightIndex;
    VkCommandBuffer             acquireCmdBuffer();

    std::vector<std::pair<VkSubmitInfo2, uint32_t>> _submitInfos;

private:
    VkCommandBuffer _currComputeCmdBuffer  = VK_NULL_HANDLE;
    VkCommandBuffer _currGraphicsCmdBuffer = VK_NULL_HANDLE;
};

} // namespace Play::RDG

#endif // RDG_H