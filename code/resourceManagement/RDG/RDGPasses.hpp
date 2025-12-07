#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP
#include <set>
#include <optional>
#include "nvvk/check_error.hpp"
#include "PlayProgram.h"
#include "RDGResources.h"
#include "utils.hpp"
#include "BaseDag.h"
namespace Play
{
class RenderPass;
}
namespace Play::RDG
{

/*
当前实现了全新的RDG pass,
RDGpass的构建交给上层逻辑pass的Build函数,pass本身只维护资源依赖逻辑,渲染逻辑以及相关的管线资源都交给上层逻辑pass管理,
由逻辑pass向底层RHI缓存进行申请
*/
using DescSetManagerRef = std::shared_ptr<Play::DescriptorSetManager>;
struct Scene;
class RenderDependencyGraph;
class RDGBuilder;
struct RenderContext;
class RenderPassBuilder;
class ComputePassBuilder;
class RTPassBuilder;

struct RDGTextureState
{
    TextureSubresourceAccessInfo textureStates;
    VkImageMemoryBarrier2        barrierInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
};

struct RDGBufferState
{
    BufferAccessInfo       bufferState;
    VkBufferMemoryBarrier2 barrierInfo{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
};

class PassNode : public Node
{
public:
    PassNode(size_t id, std::string name, NodeType type) : Node(id, type), _name(std::move(name)) {}
    void setProgram(PlayProgram* program)
    {
        _program = std::move(program);
    }

    PlayProgram* getProgram()
    {
        return _program;
    }

    void setFunc(std::function<void(RenderContext& context)> func)
    {
        _func = std::move(func);
    }

    void execute(RenderContext& context)
    {
        if (_func) _func(context);
    }

    [[nodiscard]] const std::string& name() const
    {
        return _name;
    }

    enum Type
    {
        Render,
        Compute,
        RayTracing
    };
    virtual Type type() const = 0;

    std::unordered_map<RDGTexture*, RDGTextureState>& getTextureStates()
    {
        return _textureStates;
    }
    std::unordered_map<RDGBuffer*, RDGBufferState>& getBufferStates()
    {
        return _bufferStates;
    }

protected:
    friend class RDGBuilder;
    PlayProgram*                                     _program = nullptr;
    std::function<void(RenderContext& context)>      _func;
    std::vector<VkDescriptorSet>                     _descSets;
    std::string                                      _name;
    Type                                             _type;
    std::unordered_map<RDGTexture*, RDGTextureState> _textureStates;
    std::unordered_map<RDGBuffer*, RDGBufferState>   _bufferStates;
};

class RenderPassNode : public PassNode
{
public:
    RenderPassNode(uint32_t id, std::string name);
    Type type() const override
    {
        return Type::Render;
    }
    void initRenderPass(PlayElement* element);

private:
    friend class RenderPassBuilder;
    friend class RDGBuilder;
    std::unique_ptr<RenderPass> _renderPass = nullptr;
};

using RenderPassNodeRef = RenderPassNode*;

class ComputePassNode : public PassNode
{
public:
    ComputePassNode(uint32_t id, std::string name) : PassNode(id, std::move(name), NodeType::eComputePass) {}
    void setAsyncState(bool isAsync)
    {
        _isAsync = isAsync;
    }
    [[nodiscard]]
    bool getAsyncState() const
    {
        return _isAsync;
    }
    Type type() const override
    {
        return Type::Compute;
    }

private:
    bool _isAsync = false;
    friend class ComputePassBuilder;
    friend class RDGBuilder;
};
using ComputePassNodeRef = ComputePassNode*;

class RTPassNode : public PassNode
{
public:
    RTPassNode(uint32_t id, std::string name) : PassNode(id, std::move(name), NodeType::eRTPass) {}
    Type type() const override
    {
        return Type::RayTracing;
    }

private:
    friend class RTPassBuilder;
    friend class RDGBuilder;
};
using RTPassNodeRef = RTPassNode*;

class RenderPassBuilder
{
public:
    RenderPassBuilder(RDGBuilder* builder, RenderPassNodeRef node);
    ~RenderPassBuilder() = default;
    RenderPassBuilder& program(PlayProgram* program)
    {
        _node->setProgram(std::move(program));
        return *this;
    }
    RenderPassBuilder&              color(uint32_t slotIdx, RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                          VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE, VkImageLayout initLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                          VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    RenderPassBuilder&              depthStencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                 VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE, VkImageLayout initLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    RenderPassBuilder&              read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                                         uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RenderPassBuilder&              read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage,
                                         uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    RenderPassBuilder&              readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                                              uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RenderPassBuilder&              readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage,
                                              uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    RenderPassBuilder&              execute(std::function<void(RenderContext& context)> func);
    [[nodiscard]] RenderPassNodeRef finish() const
    {
        return _node;
    }

private:
    RDGBuilder*       _builder = nullptr;
    RenderPassNodeRef _node    = nullptr;
    Dag*              _dag     = nullptr;
};

class ComputePassBuilder
{
public:
    ComputePassBuilder(RDGBuilder* builder, ComputePassNodeRef node);
    ~ComputePassBuilder() = default;
    ComputePassBuilder& program(PlayProgram* program)
    {
        _node->setProgram(program);
        return *this;
    }
    ComputePassBuilder&              read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                                          uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    ComputePassBuilder&              read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage,
                                          uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    ComputePassBuilder&              readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                                               uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    ComputePassBuilder&              readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage,
                                               uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    ComputePassBuilder&              execute(std::function<void(RenderContext& context)> func);
    ComputePassBuilder&              async(bool isAsync = false);
    [[nodiscard]] ComputePassNodeRef finish() const
    {
        return _node;
    }

private:
    RDGBuilder*        _builder = nullptr;
    ComputePassNodeRef _node    = nullptr;
    Dag*               _dag     = nullptr;
};

class RTPassBuilder
{
public:
    RTPassBuilder(RDGBuilder* builder, RTPassNodeRef node);
    ~RTPassBuilder() = default;
    RTPassBuilder& program(PlayProgram* program)
    {
        _node->setProgram(std::move(program));
        return *this;
    }
    RTPassBuilder& read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RTPassBuilder& read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    RTPassBuilder& readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                             uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RTPassBuilder& readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage,
                             uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0, size_t size = VK_WHOLE_SIZE);
    RTPassBuilder& execute(std::function<void(RenderContext& context)> func);
    [[nodiscard]] RTPassNodeRef finish() const
    {
        return _node;
    }

private:
    RDGBuilder*   _builder = nullptr;
    RTPassNodeRef _node    = nullptr;
    Dag*          _dag     = nullptr;
};

} // namespace Play::RDG

#endif // RDGPASSES_HPP