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
    RDGTextureState(RDGTextureRef textureRef, TextureSubresourceAccessInfo states, VkImageMemoryBarrier2 barrier)
        : texture(textureRef), textureStates(states), barrierInfo(barrier)
    {
    }
    RDGTextureRef                texture;
    TextureSubresourceAccessInfo textureStates;
    VkImageMemoryBarrier2        barrierInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
};

struct RDGBufferState
{
    RDGBufferState(RDGBufferRef bufferRef, BufferAccessInfo state, VkBufferMemoryBarrier2 barrier)
        : buffer(bufferRef), bufferState(state), barrierInfo(barrier)
    {
    }
    RDGBufferRef           buffer;
    BufferAccessInfo       bufferState;
    ProducerInfo           _producerInfo;
    VkBufferMemoryBarrier2 barrierInfo{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
};

class PassNode : public Node
{
public:
    PassNode(size_t id, std::string name, NodeType type) : Node(id, type), _name(std::move(name)) {}
    ~PassNode() override;

    void setFunc(std::function<void(PassNode* passNode, RenderContext& context)> func)
    {
        _func = std::move(func);
    }
    DescriptorSetBindings& getDescriptorBindings()
    {
        return _descBindings;
    }

    void execute(RenderContext& context)
    {
        if (_func) _func(this, context);
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

    std::vector<RDGTextureState>& getTextureStates()
    {
        return _textureStates;
    }
    std::vector<RDGBufferState>& getBufferStates()
    {
        return _bufferStates;
    }

    // Helper functions to find texture/buffer state in vector
    RDGTextureState* findTextureState(RDGTextureRef texture)
    {
        for (auto& state : _textureStates)
        {
            if (state.texture == texture) return &state;
        }
        return nullptr;
    }
    RDGBufferState* findBufferState(RDGBufferRef buffer)
    {
        for (auto& state : _bufferStates)
        {
            if (state.buffer == buffer) return &state;
        }
        return nullptr;
    }

protected:
    friend class RDGBuilder;
    std::function<void(PassNode* passNode, RenderContext& context)> _func;
    std::string                                                     _name;
    DescriptorSetBindings                                           _descBindings;
    Type                                                            _type;
    std::vector<RDGTextureState>                                    _textureStates;
    std::vector<RDGBufferState>                                     _bufferStates;
};

class RenderPassNode : public PassNode
{
public:
    RenderPassNode(uint32_t id, std::string name);
    Type type() const override
    {
        return Type::Render;
    }

    void        initRenderPass();
    RenderPass* getRenderPass()
    {
        return _renderPass.get();
    }
    bool isEnableMultiThreadRecording() const
    {
        return _needMultiThreadRecording;
    }

    void setMultiThreadRecordingState(bool enable)
    {
        _needMultiThreadRecording = enable;
    }

private:
    bool _needMultiThreadRecording = false;
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

struct RenderPassBuilderTraits
{
    static VkAccessFlags2 textureReadAccessMask()
    {
        return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    }

    static VkAccessFlags2 bufferReadAccessMask(RDGBufferRef)
    {
        return VK_ACCESS_2_UNIFORM_READ_BIT;
    }
};

struct ComputePassBuilderTraits
{
    static VkAccessFlags2 textureReadAccessMask()
    {
        return VK_ACCESS_2_SHADER_READ_BIT;
    }

    static VkAccessFlags2 bufferReadAccessMask(RDGBufferRef buffer)
    {
        return buffer->_info._usageFlags & VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT ? VK_ACCESS_2_UNIFORM_READ_BIT : VK_ACCESS_2_SHADER_READ_BIT;
    }
};

struct RTPassBuilderTraits
{
    static VkAccessFlags2 textureReadAccessMask()
    {
        return VK_ACCESS_2_SHADER_READ_BIT;
    }

    static VkAccessFlags2 bufferReadAccessMask(RDGBufferRef)
    {
        return VK_ACCESS_2_UNIFORM_READ_BIT;
    }
};

template <typename Derived, typename NodeRef, typename Traits>
class PassBuilderBase
{
public:
    PassBuilderBase(RDGBuilder* builder, NodeRef node) : _builder(builder), _node(node) {}
    ~PassBuilderBase() = default;

    Derived& read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        TextureSubresourceAccessInfo subResource;
        TextureAccessInfo&           accessInfo = subResource.emplace_back();
        accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding                      = binding;
        accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        accessInfo.isAttachment                 = false;
        accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        accessInfo.accessMask                   = Traits::textureReadAccessMask();
        accessInfo.stageMask                    = stage;
        accessInfo.queueFamilyIndex             = queueFamilyIndex;
        return addTextureState(texture, std::move(subResource));
    }

    Derived& read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                  uint32_t offset = 0, size_t size = VK_WHOLE_SIZE)
    {
        BufferAccessInfo accessInfo;
        accessInfo.accessMask       = Traits::bufferReadAccessMask(buffer);
        accessInfo.stageMask        = stage;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        accessInfo.offset           = offset;
        accessInfo.size             = size;
        return addBufferState(buffer, accessInfo);
    }

    Derived& storageRead(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        TextureSubresourceAccessInfo subResource;
        TextureAccessInfo&           accessInfo = subResource.emplace_back();
        accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding                      = binding;
        accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        accessInfo.isAttachment                 = false;
        accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
        accessInfo.layout                       = VK_IMAGE_LAYOUT_GENERAL;
        accessInfo.stageMask                    = stage;
        accessInfo.queueFamilyIndex             = queueFamilyIndex;
        return addTextureState(texture, std::move(subResource));
    }

    Derived& storageRead(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         uint32_t offset = 0, size_t size = VK_WHOLE_SIZE)
    {
        BufferAccessInfo accessInfo;
        accessInfo.accessMask       = VK_ACCESS_2_SHADER_READ_BIT;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        accessInfo.stageMask        = stage;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        accessInfo.offset           = offset;
        accessInfo.size             = size;
        return addBufferState(buffer, accessInfo);
    }

    Derived& storageWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        TextureSubresourceAccessInfo subResource;
        TextureAccessInfo&           accessInfo = subResource.emplace_back();
        accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding                      = binding;
        accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        accessInfo.isAttachment                 = false;
        accessInfo.accessMask                   = VK_ACCESS_2_SHADER_WRITE_BIT;
        accessInfo.layout                       = VK_IMAGE_LAYOUT_GENERAL;
        accessInfo.stageMask                    = stage;
        accessInfo.queueFamilyIndex             = queueFamilyIndex;
        return addTextureState(texture, std::move(subResource));
    }

    Derived& storageWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                          uint32_t offset = 0, size_t size = VK_WHOLE_SIZE)
    {
        BufferAccessInfo accessInfo;
        accessInfo.accessMask       = VK_ACCESS_2_SHADER_WRITE_BIT;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        accessInfo.stageMask        = stage;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        accessInfo.offset           = offset;
        accessInfo.size             = size;
        return addBufferState(buffer, accessInfo);
    }

    Derived& execute(std::function<void(PassNode* passNode, RenderContext& context)> func)
    {
        _node->setFunc(std::move(func));
        return self();
    }

    [[nodiscard]] NodeRef finish() const
    {
        return _node;
    }

protected:
    Derived& addTextureState(RDGTextureRef texture, TextureSubresourceAccessInfo subResource)
    {
        _node->getTextureStates().emplace_back(texture, std::move(subResource), VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2});
        return self();
    }

    Derived& addBufferState(RDGBufferRef buffer, const BufferAccessInfo& accessInfo)
    {
        _node->getBufferStates().emplace_back(buffer, accessInfo, VkBufferMemoryBarrier2{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2});
        return self();
    }

    Derived& self()
    {
        return static_cast<Derived&>(*this);
    }

    RDGBuilder* _builder = nullptr;
    NodeRef     _node    = nullptr;
};

class RenderPassBuilder : public PassBuilderBase<RenderPassBuilder, RenderPassNodeRef, RenderPassBuilderTraits>
{
public:
    using Base = PassBuilderBase<RenderPassBuilder, RenderPassNodeRef, RenderPassBuilderTraits>;
    using Base::Base;
    using Base::execute;
    using Base::finish;
    using Base::read;
    using Base::storageRead;
    using Base::storageWrite;
    ~RenderPassBuilder() = default;

    RenderPassBuilder& color(uint32_t slotIdx, RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                             VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    RenderPassBuilder& depth(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                             VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                             VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    RenderPassBuilder& stencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                               VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                               VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
};

class ComputePassBuilder : public PassBuilderBase<ComputePassBuilder, ComputePassNodeRef, ComputePassBuilderTraits>
{
public:
    using Base = PassBuilderBase<ComputePassBuilder, ComputePassNodeRef, ComputePassBuilderTraits>;
    using Base::Base;
    using Base::execute;
    using Base::finish;
    using Base::read;
    using Base::storageRead;
    using Base::storageWrite;
    ~ComputePassBuilder() = default;

    ComputePassBuilder& async(bool isAsync = false);
};

class RTPassBuilder : public PassBuilderBase<RTPassBuilder, RTPassNodeRef, RTPassBuilderTraits>
{
public:
    using Base = PassBuilderBase<RTPassBuilder, RTPassNodeRef, RTPassBuilderTraits>;
    using Base::Base;
    using Base::execute;
    using Base::finish;
    using Base::read;
    using Base::storageRead;
    using Base::storageWrite;
    ~RTPassBuilder() = default;
};

} // namespace Play::RDG

#endif // RDGPASSES_HPP
