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
    std::unordered_map<RDGTexture::TextureDesc, std::list<RDGTexture*>> _textureMap;
};

class RDGBuilder;
class RDGTextureBuilder
{
public:
    RDGTextureBuilder(RDGBuilder* builder, RDGTextureRef node)
        : _builder(builder), _textureNode(node)
    {
    }
    RDGTextureBuilder& Import(Texture* texture);
    RDGTextureBuilder& Format(VkFormat format);
    RDGTextureBuilder& Type(VkImageType type);
    RDGTextureBuilder& Extent(VkExtent3D extent);
    RDGTextureBuilder& UsageFlags(VkImageUsageFlags usageFlags);
    RDGTextureBuilder& AspectFlags(VkImageAspectFlags aspectFlags);
    RDGTextureBuilder& SampleCount(VkSampleCountFlagBits sampleCount);
    RDGTextureBuilder& MipmapLevel(uint32_t mipmapLevel);
    RDGTextureBuilder& LayerCount(uint32_t layerCount);
    RDGTextureRef      finish();

private:
    RDGBuilder*   _builder     = nullptr;
    RDGTextureRef _textureNode = nullptr;
};
class RDGBufferBuilder
{
public:
    RDGBufferBuilder(RDGBuilder* builder, RDGBufferRef node) : _builder(builder), _bufferNode(node)
    {
    }
    RDGBufferBuilder& Size(VkDeviceSize size);
    RDGBufferBuilder& Range(VkDeviceSize range);
    RDGBufferBuilder& UsageFlags(VkBufferUsageFlags usageFlags);
    RDGBufferBuilder& Location(bool isDeviceLocal);
    RDGBufferRef      finish();

private:
    RDGBuilder*  _builder    = nullptr;
    RDGBufferRef _bufferNode = nullptr;
};

class BlackBoard
{
public:
    BlackBoard()  = default;
    ~BlackBoard() = default;
    void          registTexture(RDGTextureRef texture);
    void          registBuffer(RDGBufferRef buffer);
    void          registPass(PassNode* pass);
    RDGTextureRef getTexture(std::string name);
    RDGBufferRef  getBuffer(std::string name);
    PassNode*     getPass(std::string name);

private:
    std::unordered_map<std::string, RDGTextureRef> _textureMap;
    std::unordered_map<std::string, RDGBufferRef>  _bufferMap;
    std::unordered_map<std::string, PassNode*>     _passMap;
};

struct PendingGfxState
{
    PendingGfxState(RenderContext* renderContext) : _renderContext(renderContext) {}
    const RenderContext* _renderContext = nullptr;
};

struct PendingComputeState
{
    PendingComputeState(RenderContext* renderContext) : _renderContext(renderContext) {}
    const RenderContext* _renderContext = nullptr;
};

struct RenderContext
{
    RenderContext(PlayElement* element) : _element(element)
    {
        _pendingComputeState = std::make_shared<PendingComputeState>(this);
        _pendingGfxState     = std::make_shared<PendingGfxState>(this);
    }
    ~RenderContext() {}
    PlayElement*                         _element;
    PlayElement::PlayFrameData*          _frameData;
    uint32_t                             _frameInFlightIndex;
    PassNode*                            _prevPassNode;
    VkCommandBuffer                      _currCmdBuffer       = VK_NULL_HANDLE;
    std::shared_ptr<PendingComputeState> _pendingComputeState = nullptr;
    std::shared_ptr<PendingGfxState>     _pendingGfxState     = nullptr;
};

class RDGBuilder
{
public:
    RDGBuilder(PlayElement* element);
    ~RDGBuilder();
    RenderPassBuilder  createRenderPass(std::string name);
    ComputePassBuilder createComputePass(std::string name);
    RTPassBuilder      createRTPass(std::string name);
    PresentPassBuilder createPresentPass();

    RDGTextureBuilder createTexture(std::string name);
    RDGBufferBuilder  createBuffer(std::string name);

    RDGTextureRef getTexture(std::string name);
    RDGBufferRef  getBuffer(std::string name);
    void          compile();
    void          execute();

    Dag* getDag()
    {
        return _dag.get();
    }

protected:
    friend class RDGTextureBuilder;
    friend class RDGBufferBuilder;
    // InputPassNodeRef createInputPass(std::string name);
    void           beforePassExecute();
    void           executePass(PassNode* pass);
    void           afterPassExecute();
    RenderContext* prepareRenderContext(PassNode* pass);
    void           prepareRenderTargets(PassNode* pass);
    void           prepareDescriptorSets(RenderContext& context, PassNode* pass);
    void           prepareRenderPass(PassNode* pass);
    friend class RenderPassBuilder;
    friend class ComputePassBuilder;
    friend class RTPassBuilder;
    friend class PresentPassBuilder;
    PlayElement*                                 _element = nullptr;
    std::unique_ptr<Dag>                         _dag     = nullptr;
    std::vector<PassNode*>                       _passes;
    BlackBoard                                   _blackBoard;
    std::unordered_map<std::string, RDGTexture*> _textureMap;
    std::unordered_map<std::string, RDGBuffer*>  _bufferMap;

private:
    std::shared_ptr<RenderContext> _renderContext;
    std::vector<std::pair<VkSubmitInfo2, uint32_t>>
        _submitInfos; // pairs of submit info and frame index
};
} // namespace Play::RDG

#endif // RDG_H