#ifndef RTRENDERER_H
#define RTRENDERER_H
#include "Renderer.h"
#include "nvvk/vulkanhppsupport.hpp"
#include "resourceManagement/Resource.h"
#include "nvh/cameramanipulator.hpp"
#include "nvvk/sbtwrapper_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"

namespace Play
{
struct Scene;
struct PlayApp;
struct Texture;
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
    void     OnDestroy() override;
    Texture* getOutputTexture() override
    {
        return _postProcessRT;
    };

   protected:
    void createPostProcessResource();
    void loadEnvTexture();
    void buildTlas();
    void buildBlas();
    void createDescritorSet();
    void rayTraceRTCreate();
    void createRenderBuffer();
    void createGraphicsPipeline();
    void createRTPipeline();
    void createPostProcessRT();
    void createPostProcessRenderPass();
    void createPostProcessFBO();
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
   Texture* _envTexture = nullptr;
   // for post processing
   VkPipeline            _postPipeline;
   VkPipelineLayout      _postPipelineLayout;
   VkDescriptorSetLayout _postDescriptorSetLayout;
   VkDescriptorSet       _postDescriptorSet;

   Texture*      _postProcessRT = nullptr;
   VkFramebuffer _postProcessFBO;
   VkRenderPass  _postProcessRenderPass;
   // for ray tracing
   Texture*                                          _rayTraceRT = nullptr;
   RenderUniform                                     _renderUniformData;
   Buffer*                                           _renderUniformBuffer = nullptr;
   uint32_t                                          _frameCount          = 0;
   std::vector<nvvk::AccelKHR>                       _blasAccels;
   VkAccelerationStructureKHR                        _tlasAccels;
   nvh::CameraManipulator::Camera                    _dirtyCamera;
   Texture*                                          _envLookupTexture = nullptr;
   VkPipeline                                        _rtPipeline;
   VkPipelineLayout                                  _rtPipelineLayout;
   std::vector<VkRayTracingShaderGroupCreateInfoKHR> _rtShaderGroups;
   VkDescriptorSetLayout                             _descriptorSetLayout;
   VkDescriptorSet                                   _descriptorSet;
   nvvk::RaytracingBuilderKHR                        _rtBuilder;
   nvvk::SBTWrapper                                  _sbtWrapper;
   nvvk::ShaderModuleManager                         _shaderModuleManager;
};
}

#endif // RTRENDERER_H