#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP
#include <set>
#include <optional>
#include "PlayProgram.h"
#include "RDGResources.h"
#include "RDGPreDefine.h"
#include "utils.hpp"
#include "BaseDag.h"
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

class RootSignature
{
public:
    RootSignature();
    ~RootSignature();
    void addBinding(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,
                    uint32_t descriptorCount, VkShaderStageFlags stageFlags,
                    const VkSampler*         pImmutableSamplers = nullptr,
                    VkDescriptorBindingFlags bindingFlags       = 0);
    void clear();
    void clear(uint32_t set);
    nvvk::DescriptorBindings& getBinding(uint32_t set);
    VkPushConstantRange&      PushConstantRange()
    {
        return _pushConstantRange;
    }

private:
    std::vector<nvvk::DescriptorBindings> _descritorBindings;
    VkPushConstantRange                   _pushConstantRange;
};

class PassNode : public Node
{
public:
    PassNode(size_t id, std::string name) : Node(id), _name(std::move(name))
    {
        m_priority = NodePriority::ePrimary;
    }
    void setRootSignature(const RootSignature& rootSignature)
    {
        _rootSignature = rootSignature;
    }

private:
    RootSignature                _rootSignature;
    std::vector<VkDescriptorSet> _descSets;
    std::string                  _name;
};

class RenderPassNode : public PassNode
{
public:
    RenderPassNode(uint32_t id, std::string name) : PassNode(id, std::move(name)) {}
};

using RenderPassNodeRef = RenderPassNode*;

class ComputePassNode : public PassNode
{
public:
    ComputePassNode(uint32_t id, std::string name) : PassNode(id, std::move(name)) {}
};
using ComputePassNodeRef = ComputePassNode*;

class RTPassNode : public PassNode
{
public:
    RTPassNode(uint32_t id, std::string name) : PassNode(id, std::move(name)) {}
};
using RTPassNodeRef = RTPassNode*;

class RenderPassBuilder
{
public:
    RenderPassBuilder(RDGBuilder* builder, RenderPassNodeRef node);
    ~RenderPassBuilder() = default;
    RenderPassBuilder& color(uint32_t binding, TextureNodeRef texHandle,
                             VkAttachmentLoadOp  loadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR,
                             VkAttachmentStoreOp storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                             VkImageLayout       initLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                             VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    RenderPassBuilder& depthStencil(
        TextureNodeRef texHandle, VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentStoreOp storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        VkImageLayout       initLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        VkImageLayout       finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    RenderPassBuilder& read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                            VkPipelineStageFlagBits2 stage,
                            uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RenderPassBuilder& read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                            VkPipelineStageFlagBits2 stage,
                            uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            uint32_t offset = 0, uint32_t size = VK_WHOLE_SIZE);
    RenderPassBuilder& readWrite(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                 VkPipelineStageFlagBits2 stage,
                                 uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RenderPassBuilder& readWrite(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                 VkPipelineStageFlagBits2 stage,
                                 uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                 uint32_t offset = 0, uint32_t size = VK_WHOLE_SIZE);
    RenderPassNodeRef  finish()
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
    ComputePassBuilder& read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                             VkPipelineStageFlagBits2 stage,
                             uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    ComputePassBuilder& read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                             VkPipelineStageFlagBits2 stage,
                             uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                             uint32_t offset = 0, uint32_t size = VK_WHOLE_SIZE);
    ComputePassBuilder& readWrite(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                  VkPipelineStageFlagBits2 stage,
                                  uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    ComputePassBuilder& readWrite(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                  VkPipelineStageFlagBits2 stage,
                                  uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                  uint32_t offset = 0, uint32_t size = VK_WHOLE_SIZE);
    ComputePassNodeRef  finish()
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
    RTPassBuilder& read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                        VkPipelineStageFlagBits2 stage,
                        uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RTPassBuilder& read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                        VkPipelineStageFlagBits2 stage,
                        uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t offset = 0,
                        uint32_t size = VK_WHOLE_SIZE);
    RTPassBuilder& readWrite(uint32_t set, uint32_t binding, TextureNodeRef texture,
                             VkPipelineStageFlagBits2 stage,
                             uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
    RTPassBuilder& readWrite(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                             VkPipelineStageFlagBits2 stage,
                             uint32_t                 queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                             uint32_t offset = 0, uint32_t size = VK_WHOLE_SIZE);
    RTPassNodeRef  finish()
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