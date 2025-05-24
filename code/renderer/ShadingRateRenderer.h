#ifndef SHADINGRATERENDERER_H
#define SHADINGRATERENDERER_H
#include "Renderer.h"
#include "VolumeRenderer.h"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/compute_vk.hpp"
#include <vulkan/vulkan_core.h>
namespace Play{

class Buffer;

class ShadingRateRenderer : public Renderer
{
public:
  explicit ShadingRateRenderer(PlayApp& app);
  ~ShadingRateRenderer() override = default;
  void OnPreRender() override;
  void OnPostRender() override;
  void RenderFrame() override;
  void SetScene(Scene* scene) override;
  void OnResize(int width, int height) override;
  void OnDestroy() override;
  Texture* getOutputTexture() override{
      return _outputTexture;
  }

protected:
  struct ShaderRateUniformStruct
  {
    glm::mat4 ProjectMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 WorldMatrix;

    glm::mat4 InvWorldMatrix;
    glm::mat4 InvProjectMatrix;
    glm::mat4 InvViewMatrix;
    glm::vec3 CameraPos;
    int test = 0;
  }_ShaderRateUniformStruct;
  struct ComputeUniformStruct{
    glm::uvec2 FrameSize;
    glm::uvec2 ShadingRateSize;
    glm::uvec2 maxRates;
    uint32_t n_rates;
  }_ComputeUniformStruct;
  void initPipeline();
  void createRenderResource();
  void createRenderPass();
  void createFrameBuffer();
  struct DepthTexture
  {
    VkImage image;
    VkDeviceMemory memory;
    VkDescriptorImageInfo descriptor;
  }_depthTexture;

  ComputePass _computePass;
  Texture* _shadingRateTexture = nullptr;
  Texture* _outputTexture = nullptr;
  Texture* _gradientTexture = nullptr;
  Buffer* _renderUniformBuffer = nullptr;
  Buffer* _computeUniformBuffer = nullptr;
  std::vector<VkWriteDescriptorSet> _writeDescriptorSets;
  VkPipelineLayout _razePipelineLayout = VK_NULL_HANDLE;
  VkPipeline _razePipeline = VK_NULL_HANDLE;
  VkRenderPass _shadingRateRenderPass = VK_NULL_HANDLE;
  VkFramebuffer _shadingRateFramebuffer = VK_NULL_HANDLE;
  VkDescriptorSet _sceneInstanceSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout _sceneInstanceSetLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _shadingRateSetLayout = VK_NULL_HANDLE;
  VkPhysicalDeviceFragmentShadingRatePropertiesKHR    _physical_device_fragment_shading_rate_properties{};
  std::vector<VkPhysicalDeviceFragmentShadingRateKHR> fragment_shading_rates{};
  nvh::CameraManipulator::Camera _dirtyCamera;
  VkExtent2D _shadingRateExtent;
};

} // namespace Play

#endif // #include "Renderer.h"