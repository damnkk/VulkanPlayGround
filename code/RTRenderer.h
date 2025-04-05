#ifndef RTRENDERER_H
#define RTRENDERER_H
#include "Renderer.h"
#include "nvvk/vulkanhppsupport.hpp"
#include "Resource.h"
#include "nvh/cameramanipulator.hpp"
#include "nvvk/sbtwrapper_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"

namespace Play
{
struct Scene;
struct PlayApp;
class RTRenderer : public Renderer
{
public:
    RTRenderer(PlayApp& app);
    ~RTRenderer() override = default;

    void OnPreRender() override;
    void OnPostRender() override;
    void RenderFrame() override;
    void SetScene(Scene* scene) override;
    void OnResize(int width, int height) override;
    void OnDestroy() override;

   protected:
   void loadEnvTexture();
   void buildTlas();
   void buildBlas();
   void createDescritorSet();
   void rayTraceRTCreate();
   void createRenderBuffer();
   void createGraphicsPipeline();
   void createRTPipeline();
   void createRazterizationRenderPass();
   void createRazterizationFBO();
   void createPostPipeline();
   void createPostDescriptorSet();

   private:
   enum ObjBinding
   {
       eTlas,
       eRayTraceRT,
       eMaterialBuffer,
       eRenderUniform,
       eLightMeshIdx,
       eInstanceBuffer,
       // ePrimitiveBuffer,
       eSceneTexture,
       eEnvTexture,
       eEnvLoopupTexture,
       eCount
   };
    // env texture
    Texture _envTexture;
    // for post processing
    VkPipeline            _postPipeline;
    VkPipelineLayout      _postPipelineLayout;
    VkDescriptorSetLayout _postDescriptorSetLayout;
    VkDescriptorSet       _postDescriptorSet;
    struct DepthTexture
    {
        VkImage        image;
        VkDeviceMemory memory;
        VkImageView    view;
        VkImageLayout  layout;
    } _rasterizationDepthImage;
    VkFramebuffer _rasterizationFBO;
    VkRenderPass  _rasterizationRenderPass;
    // for ray tracing
    Texture                     _rayTraceRT;
    RenderUniform               _renderUniformData;
    Buffer                      _renderUniformBuffer;
    uint32_t                    _frameCount = 0;
    std::vector<nvvk::AccelKHR> _blasAccels;
    VkAccelerationStructureKHR  _tlasAccels;
    nvh::CameraManipulator::Camera _dirtyCamera;
    Texture                        _envLookupTexture;
    VkPipeline                                        _rtPipeline;
    VkPipelineLayout                                  _rtPipelineLayout;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> _rtShaderGroups;
    VkDescriptorSetLayout _descriptorSetLayout;
    VkDescriptorSet                                   _descriptorSet;
    nvvk::RaytracingBuilderKHR _rtBuilder;
    nvvk::SBTWrapper           _sbtWrapper;
    nvvk::ShaderModuleManager  _shaderModuleManager;
};
}

#endif // RTRENDERER_H