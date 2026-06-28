#ifndef VOLUME_RENDER_PASS_H
#define VOLUME_RENDER_PASS_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include "RenderPass.h"
#include "core/RefCounted.h"
#include "PipelineCacheManager.h"

namespace Play
{
class Buffer;
class Texture;
class VolumeRenderer;

struct VolumeRenderParameters
{
    float     Density   = 100.0f;
    float     Exposure  = 1.0f;
    uint32_t  StepCount = 180;
    glm::vec3 BBoxMin   = glm::vec3(-0.5f, -0.5f, -0.65f);
    glm::vec3 BBoxMax   = glm::vec3(0.5f, 0.5f, 0.65f);
};

struct VolumeUniformData
{
    glm::mat4 ProjectMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 WorldMatrix;
    glm::mat4 NormalMatrix;

    glm::mat4 InvProjectMatrix;
    glm::mat4 InvViewMatrix;
    glm::mat4 InvWorldMatrix;
    glm::mat4 InvNormalMatrix;

    uint32_t  frameCount;
    float     StepSize;
    glm::vec2 FrameOffset;

    glm::vec2 RTDimensions;
    float     Density;
    float     Exposure;
    glm::vec3 CameraPos;
    glm::vec3 BBoxMin;
    glm::vec3 BBoxMax;
};

class VolumeRenderPass : public BasePass
{
public:
    VolumeRenderPass() = default;
    explicit VolumeRenderPass(VolumeRenderer* renderer);
    ~VolumeRenderPass() override;

    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;
    void updateUniform();
    void resetFrameCount();

    RTTR_ENABLE(BasePass)

private:
    enum TextureSlot : uint32_t
    {
        eVolumeTexture,
        eGradientTexture,
        eDiffuseLookUpTexture,
        eSpecularLookUpTexture,
        eRoughnessLookUpTexture,
        eOpacityLookUpTexture,
        eEnvTexture,
        eCount
    };

    void loadVolumeTexture();
    void createTransferFunctionTextures();
    void createEnvTexture();
    void createUniformBuffer();
    void uploadTexture(Texture& texture, const void* data, VkDeviceSize dataSize, VkImageCreateInfo imageInfo, VkImageViewCreateInfo viewInfo,
                       VkImageLayout finalLayout);
    void acquireLinearSampler(Texture& texture);
    bool parametersChanged() const;

    VolumeRenderer* _ownedRenderer = nullptr;

    ComputePipelineStateInitializer _gradientPipeline;
    ComputePipelineStateInitializer _generateRaysPipeline;
    ComputePipelineStateInitializer _radiancePipeline;
    ComputePipelineStateInitializer _accumulatePipeline;
    ComputePipelineStateInitializer _postProcessPipeline;

    RefPtr<Texture> _textures[static_cast<uint32_t>(TextureSlot::eCount)];
    RefPtr<Buffer>  _uniformBuffer;
    VkExtent3D      _volumeExtent = {1, 1, 1};

    VolumeRenderParameters         _parameters;
    mutable VolumeRenderParameters _lastParameters;
    glm::mat4                      _lastViewMatrix    = glm::mat4(0.0f);
    glm::mat4                      _lastProjectMatrix = glm::mat4(0.0f);
    uint32_t                       _frameCount        = 0;
    bool                           _gradientGenerated = false;
};

} // namespace Play

#endif // VOLUME_RENDER_PASS_H
