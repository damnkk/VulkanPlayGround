#include "GBufferPass.h"

#include "DeferRendering.h"
#include "ShaderManager.hpp"
#include "PConstantType.h.slang"
#include "PlayAllocator.h"
#include "RDG/RDG.h"
#include "SceneManager.h"
#include "core/runtime/VulkanRuntime.h"
#include "utils.hpp"

namespace Play
{

namespace
{

constexpr VkBufferUsageFlags2 kGBufferGPUInstanceDataUsage    = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
constexpr uint32_t            kGBufferInstanceFlagDoubleSided = 1 << 0;
constexpr uint32_t            kGBufferColorAttachmentCount    = 6;

bool isBoundsInFrustum(const AABB& bounds, const glm::mat4& viewProj)
{
    const glm::vec3 corners[] = {
        {bounds.min.x, bounds.min.y, bounds.min.z}, {bounds.max.x, bounds.min.y, bounds.min.z}, {bounds.min.x, bounds.max.y, bounds.min.z},
        {bounds.max.x, bounds.max.y, bounds.min.z}, {bounds.min.x, bounds.min.y, bounds.max.z}, {bounds.max.x, bounds.min.y, bounds.max.z},
        {bounds.min.x, bounds.max.y, bounds.max.z}, {bounds.max.x, bounds.max.y, bounds.max.z},
    };

    uint32_t outsideLeft   = 0;
    uint32_t outsideRight  = 0;
    uint32_t outsideBottom = 0;
    uint32_t outsideTop    = 0;
    uint32_t outsideNear   = 0;
    uint32_t outsideFar    = 0;

    for (uint32_t i = 0; i < 8; ++i)
    {
        const glm::vec4 clip = viewProj * glm::vec4(corners[i], 1.0f);
        if (clip.x < -clip.w) ++outsideLeft;
        if (clip.x > clip.w) ++outsideRight;
        if (clip.y < -clip.w) ++outsideBottom;
        if (clip.y > clip.w) ++outsideTop;
        if (clip.z < 0.0f) ++outsideNear;
        if (clip.z > clip.w) ++outsideFar;
    }

    return outsideLeft < 8 && outsideRight < 8 && outsideBottom < 8 && outsideTop < 8 && outsideNear < 8 && outsideFar < 8;
}

float computeDepthKey(const AABB& bounds, const CameraData& cameraData)
{
    const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    return glm::length(center - cameraData.cameraPosition);
}

uint32_t quantizeDepth(float depth)
{
    if (depth <= 0.0f)
    {
        return 0;
    }

    const float scaledDepth = depth * 16.0f;
    if (scaledDepth >= 1048575.0f)
    {
        return 1048575u;
    }

    return static_cast<uint32_t>(scaledDepth);
}

uint64_t makeSortKey(float depthKey, uint32_t materialIndex, uint32_t meshInfoIndex)
{
    const uint64_t depthBits    = static_cast<uint64_t>(quantizeDepth(depthKey)) & 0xFFFFFull;
    const uint64_t materialBits = static_cast<uint64_t>(materialIndex) & 0xFFFFFull;
    const uint64_t meshBits     = static_cast<uint64_t>(meshInfoIndex) & 0xFFFFFull;
    return (depthBits << 40) | (materialBits << 20) | meshBits;
}

uint64_t meshInfoAddressForModel(const ModelAsset& model, const GpuModelRange& range, uint32_t meshInfoIndex)
{
    if (!model.meshInfoBuffer || meshInfoIndex < range.firstMeshInfo)
    {
        return 0;
    }

    const uint32_t localMeshInfoIndex = meshInfoIndex - range.firstMeshInfo;
    if (localMeshInfoIndex >= range.meshInfoCount)
    {
        return 0;
    }

    return model.meshInfoBuffer->address + localMeshInfoIndex * sizeof(MeshInfo);
}

uint64_t materialAddressForModel(const ModelAsset& model, const GpuModelRange& range, uint32_t materialIndex)
{
    if (!model.materialBuffer || materialIndex < range.firstMaterial)
    {
        return 0;
    }

    const uint32_t localMaterialIndex = materialIndex - range.firstMaterial;
    if (localMaterialIndex >= range.materialCount)
    {
        return 0;
    }

    return model.materialBuffer->address + localMaterialIndex * sizeof(shaderio::GltfShadeMaterial);
}

uint64_t textureInfoAddressForModel(const ModelAsset& model, const GpuModelRange& range)
{
    if (!model.textureInfoBuffer || range.textureInfoCount == 0)
    {
        return 0;
    }

    return model.textureInfoBuffer->address;
}

} // namespace

void GBufferPass::init()
{
    const uint32_t vertexShaderID = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME);
    const uint32_t fragShaderID   = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME);

    _gbufferPipeline.setShader(vertexShaderID, fragShaderID);
    _gbufferPipeline.setPushConstant<GBufferPushConstant>();
    _gbufferPipeline.psoState.colorBlendEnables.resize(kGBufferColorAttachmentCount, VK_FALSE);
    _gbufferPipeline.psoState.colorWriteMasks.resize(kGBufferColorAttachmentCount,
                                                     VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                                         VK_COLOR_COMPONENT_A_BIT);
    const VkColorBlendEquationEXT defaultBlendEquation = _gbufferPipeline.psoState.colorBlendEquations.front();
    _gbufferPipeline.psoState.colorBlendEquations.resize(kGBufferColorAttachmentCount, defaultBlendEquation);
}

void GBufferPass::prepareRenderList()
{
    _visibleInstances.clear();
    _renderItems.clear();
    _gpuInstanceData.clear();

    if (!_ownedRender || !_ownedRender->getSceneManager())
    {
        return;
    }

    SceneManager*   sceneManager = _ownedRender->getSceneManager();
    const GpuScene* gpuScene     = sceneManager->getGpuScene();
    if (!gpuScene)
    {
        return;
    }

    const CameraData& cameraData = _ownedRender->getCurrentCameraData();
    sceneManager->readSceneGraph([&](const CpuScene& scene) { collectVisibleInstances(scene, *gpuScene, cameraData); });

    buildRenderList(*gpuScene);
    sortRenderList();
    uploadGPUInstanceData();
}

void GBufferPass::collectVisibleInstances(const CpuScene& scene, const GpuScene& gpuScene, const CameraData& cameraData)
{
    const std::vector<ModelAsset>& models = gpuScene.getModels();

    const std::vector<CpuSceneNode>& nodes = scene.getNodes();
    for (uint32_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const CpuSceneNode& node = nodes[nodeIndex];
        if (!node.alive || !node.worldVisible)
        {
            continue;
        }

        for (CpuSceneComponentID componentID : node.components)
        {
            const CpuModelComponent* modelComponent = scene.getComponent<CpuModelComponent>(componentID);
            if (!modelComponent || !modelComponent->visible || !modelComponent->hasModel())
            {
                continue;
            }

            if (modelComponent->model.index >= models.size())
            {
                continue;
            }

            const ModelAsset& model = models[modelComponent->model.index];
            if (model.generation != modelComponent->model.generation || model.renderables.empty())
            {
                continue;
            }

            const uint32_t firstRenderable = modelComponent->firstRenderable;
            if (firstRenderable >= model.renderables.size())
            {
                continue;
            }

            const uint32_t availableRenderables = static_cast<uint32_t>(model.renderables.size()) - firstRenderable;
            uint32_t       renderableCount      = modelComponent->usesAllRenderables() ? availableRenderables : modelComponent->renderableCount;
            if (renderableCount > availableRenderables)
            {
                renderableCount = availableRenderables;
            }
            if (renderableCount == 0)
            {
                continue;
            }

            AABB localBounds;
            bool hasBounds = false;
            for (uint32_t renderableOffset = 0; renderableOffset < renderableCount; ++renderableOffset)
            {
                const ModelRenderableTemplate& renderable = model.renderables[firstRenderable + renderableOffset];
                if (hasBounds)
                {
                    expandAABB(localBounds, renderable.modelBounds);
                }
                else
                {
                    localBounds = renderable.modelBounds;
                    hasBounds   = true;
                }
            }
            if (!hasBounds)
            {
                continue;
            }

            GBufferVisibleInstance visibleInstance;
            visibleInstance.modelIndex      = modelComponent->model.index;
            visibleInstance.firstRenderable = firstRenderable;
            visibleInstance.renderableCount = renderableCount;
            visibleInstance.objectToWorld   = node.worldTransform;
            visibleInstance.worldBounds     = transformAABB(localBounds, node.worldTransform);
            visibleInstance.depthKey        = computeDepthKey(visibleInstance.worldBounds, cameraData);

            if (!isBoundsInFrustum(visibleInstance.worldBounds, cameraData.viewProjMatrix))
            {
                continue;
            }

            _visibleInstances.push_back(visibleInstance);
        }
    }
}

void GBufferPass::buildRenderList(const GpuScene& gpuScene)
{
    const GpuSceneCommonData&         common      = gpuScene.getCommonData();
    const std::vector<ModelAsset>&    models      = gpuScene.getModels();
    const std::vector<GpuModelRange>& modelRanges = gpuScene.getModelRanges();

    for (uint32_t visibleIndex = 0; visibleIndex < _visibleInstances.size(); ++visibleIndex)
    {
        const GBufferVisibleInstance& visibleInstance = _visibleInstances[visibleIndex];
        if (visibleInstance.modelIndex >= models.size() || visibleInstance.modelIndex >= modelRanges.size())
        {
            continue;
        }

        const ModelAsset&    model = models[visibleInstance.modelIndex];
        const GpuModelRange& range = modelRanges[visibleInstance.modelIndex];

        for (uint32_t renderableOffset = 0; renderableOffset < visibleInstance.renderableCount; ++renderableOffset)
        {
            const uint32_t renderableIndex = visibleInstance.firstRenderable + renderableOffset;
            if (renderableIndex >= model.renderables.size())
            {
                continue;
            }

            const ModelRenderableTemplate& renderable = model.renderables[renderableIndex];
            if (renderable.submeshIndex >= model.submeshes.size())
            {
                continue;
            }

            const ModelSubmeshAsset& submesh       = model.submeshes[renderable.submeshIndex];
            const uint32_t           meshInfoIndex = submesh.meshID;
            if (meshInfoIndex == INVALID_SCENE_ID || meshInfoIndex >= common.meshInfos.size())
            {
                continue;
            }

            const MeshInfo& meshInfo = common.meshInfos[meshInfoIndex];
            if (meshInfo.indexCount == 0)
            {
                continue;
            }

            GBufferGPUInstanceData gpuInstanceData;
            gpuInstanceData.objectToWorld      = visibleInstance.objectToWorld * renderable.localToModel;
            gpuInstanceData.worldToObject      = glm::inverse(gpuInstanceData.objectToWorld);
            gpuInstanceData.meshInfoAddress    = meshInfoAddressForModel(model, range, meshInfoIndex);
            gpuInstanceData.materialAddress    = materialAddressForModel(model, range, meshInfo.materialIdx);
            gpuInstanceData.textureInfoAddress = textureInfoAddressForModel(model, range);
            gpuInstanceData.meshInfoIndex      = meshInfoIndex;
            gpuInstanceData.materialIndex      = meshInfo.materialIdx;
            gpuInstanceData.textureInfoOffset  = range.firstTextureInfo;

            if (meshInfo.materialIdx < common.materials.size() && common.materials[meshInfo.materialIdx].doubleSided != 0)
            {
                gpuInstanceData.flags |= kGBufferInstanceFlagDoubleSided;
            }

            GBufferRenderItem renderItem;
            renderItem.visibleInstanceIndex = visibleIndex;
            renderItem.renderableIndex      = renderableIndex;
            renderItem.meshInfoIndex        = meshInfoIndex;
            renderItem.materialIndex        = meshInfo.materialIdx;
            renderItem.indexCount           = meshInfo.indexCount;
            renderItem.depthKey             = visibleInstance.depthKey;
            renderItem.sortKey              = makeSortKey(renderItem.depthKey, renderItem.materialIndex, renderItem.meshInfoIndex);
            renderItem.gpuInstanceIndex     = static_cast<uint32_t>(_gpuInstanceData.size());

            _gpuInstanceData.push_back(gpuInstanceData);
            _renderItems.push_back(renderItem);
        }
    }
}

void GBufferPass::sortRenderList()
{
    std::sort(_renderItems.begin(), _renderItems.end(),
              [](const GBufferRenderItem& lhs, const GBufferRenderItem& rhs)
              {
                  if (lhs.sortKey == rhs.sortKey)
                  {
                      return lhs.depthKey < rhs.depthKey;
                  }
                  return lhs.sortKey < rhs.sortKey;
              });
}

void GBufferPass::uploadGPUInstanceData()
{
    if (_renderItems.empty())
    {
        _gpuInstanceData.clear();
        return;
    }

    std::vector<GBufferGPUInstanceData> sortedInstanceData;
    sortedInstanceData.reserve(_renderItems.size());
    for (uint32_t itemIndex = 0; itemIndex < _renderItems.size(); ++itemIndex)
    {
        GBufferRenderItem& renderItem = _renderItems[itemIndex];
        if (renderItem.gpuInstanceIndex >= _gpuInstanceData.size())
        {
            continue;
        }

        sortedInstanceData.push_back(_gpuInstanceData[renderItem.gpuInstanceIndex]);
        renderItem.gpuInstanceIndex = static_cast<uint32_t>(sortedInstanceData.size() - 1);
    }
    _gpuInstanceData.swap(sortedInstanceData);

    const VkDeviceSize dataSize = static_cast<VkDeviceSize>(_gpuInstanceData.size() * sizeof(GBufferGPUInstanceData));
    if (dataSize == 0)
    {
        return;
    }

    if (!_gpuInstanceDataBuffer || _gpuInstanceDataBuffer->BufferSize() < dataSize)
    {
        _gpuInstanceDataBuffer = RefPtr<Buffer>(new Buffer("GBufferGPUInstanceData", kGBufferGPUInstanceDataUsage, dataSize,
                                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    }

    if (_gpuInstanceDataBuffer && _gpuInstanceDataBuffer->mapping)
    {
        memcpy(_gpuInstanceDataBuffer->mapping, _gpuInstanceData.data(), dataSize);
        PlayResourceManager::Instance().flushBuffer(*_gpuInstanceDataBuffer, 0, dataSize);
    }
}

void GBufferPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGTextureRef BaseColorRT = rdgBuilder->getTexture("SkyBoxRT");

    RDG::RDGTextureRef WorldNormalRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GNormal).debugName)
                                           .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                           .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                           .Format(GBufferConfig::Get(GBufferType::GNormal).format)
                                           .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                           .MipmapLevel(1)
                                           .finish();

    RDG::RDGTextureRef PBRRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GPBR).debugName)
                                   .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                   .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                   .Format(GBufferConfig::Get(GBufferType::GPBR).format)
                                   .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                   .MipmapLevel(1)
                                   .finish();

    RDG::RDGTextureRef EmissiveRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GEmissive).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GEmissive).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();

    RDG::RDGTextureRef Custom1RT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GCustomData).debugName)
                                       .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                       .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                       .Format(GBufferConfig::Get(GBufferType::GCustomData).format)
                                       .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                       .MipmapLevel(1)
                                       .finish();

    RDG::RDGTextureRef VelocityRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GVelocity).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GVelocity).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();

    const auto         depthFormat = GBufferConfig::Get(GBufferType::GSceneDepth).format;
    RDG::RDGTextureRef DepthRT     = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GSceneDepth).debugName)
                                     .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                     .AspectFlags(inferImageAspectFlags(depthFormat, false))
                                     .Format(depthFormat)
                                     .UsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                     .MipmapLevel(1)
                                     .finish();

    auto pass =
        rdgBuilder->createRenderPass("GBufferPass")
            .color(0, BaseColorRT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .color(1, WorldNormalRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .color(2, PBRRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .color(3, EmissiveRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .color(4, Custom1RT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .color(5, VelocityRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .depth(DepthRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .execute(
                [this](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    prepareRenderList();

                    if (_renderItems.empty())
                    {
                        return;
                    }
                    VkCommandBuffer cmd = context._currCmdBuffer;

                    GBufferPushConstant pushConstant{};
                    pushConstant.perFrameConstant.cameraBufferDeviceAddress = _ownedRender->getCurrentCameraBuffer()->address;
                    pushConstant.sceneConstant.instanceBufferAddress        = _gpuInstanceDataBuffer ? _gpuInstanceDataBuffer->address : 0;

                    context.bindPipeline(_gbufferPipeline);

                    VkViewport viewport = {
                        0,    0,   static_cast<float>(vkDriver->getViewportSize().width), static_cast<float>(vkDriver->getViewportSize().height),
                        0.0f, 1.0f};
                    VkRect2D scissor = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);

                    for (const GBufferRenderItem& item : _renderItems)
                    {
                        if (item.indexCount == 0)
                        {
                            continue;
                        }

                        pushConstant.sceneConstant.instanceIndex = item.gpuInstanceIndex;
                        context.bindPushConstant(pushConstant);
                        vkCmdDraw(cmd, item.indexCount, 1, 0, 0);
                    }
                })
            .finish();
}

} // namespace Play
