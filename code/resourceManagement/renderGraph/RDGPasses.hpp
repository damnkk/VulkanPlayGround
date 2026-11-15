#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP
#include <set>
#include <optional>
#include "nvvk/check_error.hpp"

#include "resourceManagement/vulkan/descriptors/DescriptorManager.h"
#include "RDGResources.h"

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
struct Scene;
class RenderDependencyGraph;
class RDGBuilder;
struct RenderContext;
class RenderPassBuilder;
class ComputePassBuilder;
class RTPassBuilder;

struct RDGTextureState
{
    RDGTextureRef     texture;
    TextureAccessInfo use;
};

struct RDGBufferState
{
    RDGBufferRef     buffer;
    BufferAccessInfo bufferState;
};

class PassNode
{
public:
    explicit PassNode(std::string name) : _name(std::move(name)) {}
    virtual ~PassNode();

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

protected:
    friend class RDGBuilder;
    std::function<void(PassNode* passNode, RenderContext& context)> _func;
    std::string                                                     _name;
    DescriptorSetBindings                                           _descBindings;
    std::vector<RDGTextureState>                                    _textureStates;
    std::vector<RDGBufferState>                                     _bufferStates;
};

class RenderPassNode : public PassNode
{
public:
    explicit RenderPassNode(std::string name);
    ~RenderPassNode() override;
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
    using PassNode::PassNode;
    Type type() const override
    {
        return Type::Compute;
    }

    friend class RDGBuilder;
};
using ComputePassNodeRef = ComputePassNode*;

class RTPassNode : public PassNode
{
public:
    using PassNode::PassNode;
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
        return readTexture(binding, texture, stage, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, queueFamilyIndex);
    }

    Derived& sampledRead(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        return readTexture(binding, texture, stage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, queueFamilyIndex);
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
        TextureAccessInfo accessInfo;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        accessInfo.isAttachment     = false;
        accessInfo.accessMask       = VK_ACCESS_2_SHADER_READ_BIT;
        accessInfo.layout           = VK_IMAGE_LAYOUT_GENERAL;
        accessInfo.stageMask        = stage;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        return addTextureState(texture, accessInfo);
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

    Derived& storageWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                          uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        TextureAccessInfo accessInfo;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        accessInfo.isAttachment     = false;
        accessInfo.accessMask       = VK_ACCESS_2_SHADER_WRITE_BIT;
        accessInfo.layout           = VK_IMAGE_LAYOUT_GENERAL;
        accessInfo.stageMask        = stage;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        return addTextureState(texture, accessInfo);
    }

    Derived& storageReadWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage,
                              uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
    {
        TextureAccessInfo accessInfo;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        accessInfo.isAttachment     = false;
        accessInfo.accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        accessInfo.layout           = VK_IMAGE_LAYOUT_GENERAL;
        accessInfo.stageMask        = stage;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        return addTextureState(texture, accessInfo);
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
    Derived& readTexture(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, VkDescriptorType descriptorType,
                         uint32_t queueFamilyIndex)
    {
        TextureAccessInfo accessInfo;
        accessInfo.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
        accessInfo.binding          = binding;
        accessInfo.descriptorType   = descriptorType;
        accessInfo.isAttachment     = false;
        accessInfo.layout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        accessInfo.accessMask       = Traits::textureReadAccessMask();
        accessInfo.stageMask        = stage;
        accessInfo.queueFamilyIndex = queueFamilyIndex;
        return addTextureState(texture, accessInfo);
    }

    Derived& addTextureState(RDGTextureRef texture, const TextureAccessInfo& accessInfo)
    {
        _node->getTextureStates().push_back({texture, accessInfo});
        return self();
    }

    Derived& addBufferState(RDGBufferRef buffer, const BufferAccessInfo& accessInfo)
    {
        _node->getBufferStates().push_back({buffer, accessInfo});
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
    using Base::sampledRead;
    using Base::storageRead;
    using Base::storageReadWrite;
    using Base::storageWrite;
    ~RenderPassBuilder() = default;

    RenderPassBuilder& color(uint32_t slotIdx, RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                             VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    RenderPassBuilder& depth(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                             VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                             VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                             VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    RenderPassBuilder& stencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                               VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                               VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
};

class ComputePassBuilder : public PassBuilderBase<ComputePassBuilder, ComputePassNodeRef, ComputePassBuilderTraits>
{
public:
    using Base = PassBuilderBase<ComputePassBuilder, ComputePassNodeRef, ComputePassBuilderTraits>;
    using Base::Base;
    using Base::execute;
    using Base::finish;
    using Base::read;
    using Base::sampledRead;
    using Base::storageRead;
    using Base::storageReadWrite;
    using Base::storageWrite;
    ~ComputePassBuilder() = default;
};

class RTPassBuilder : public PassBuilderBase<RTPassBuilder, RTPassNodeRef, RTPassBuilderTraits>
{
public:
    using Base = PassBuilderBase<RTPassBuilder, RTPassNodeRef, RTPassBuilderTraits>;
    using Base::Base;
    using Base::execute;
    using Base::finish;
    using Base::read;
    using Base::sampledRead;
    using Base::storageRead;
    using Base::storageReadWrite;
    using Base::storageWrite;
    ~RTPassBuilder() = default;
};

} // namespace Play::RDG

#endif // RDGPASSES_HPP
