#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvvkhl/appbase_vk.hpp"
#include "ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include "nvvk/memallocator_vma_vk.hpp"
#include "nvvk/acceleration_structures.hpp"
#include "Resource.h"
#include "SceneNode.h"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"
namespace Play
{

struct RTRenderer;
class PlayApp : public nvvkhl::AppBaseVk
{
   public:
    void   onResize(int width, int height) override;
    void   onDestroy();
    void   OnInit();
    void   Run();
    void   RenderFrame();
    void   OnPreRender();
    void   OnPostRender();
    Scene& getScene()
    {
        return _scene;
    };

    virtual void onKeyboard(int key, int scancode, int action, int mods) override;
    static inline Texture& AllocTexture();
    static inline void     FreeTexture(Texture& texture);
    static inline Buffer&  AllocBuffer();
    static inline void     FreeBuffer(Buffer& buffer);

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
    virtual void updateInputs() override;

   private:
    friend class RTRenderer;
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

    enum RenderMode
    {
        eRasterization,
        eRayTracing,
        eRCount
    } _renderMode = eRayTracing;
    friend class ModelLoader;
    ModelLoader _modelLoader;
    nvvk::ResourceAllocatorVma _alloc;
    nvvk::RaytracingBuilderKHR _rtBuilder;
    nvvk::SBTWrapper           _sbtWrapper;
    nvvk::ShaderModuleManager  _shaderModuleManager;
    nvvk::DebugUtil            m_debug;
    static TexturePool         _texturePool;
    static BufferPool          _bufferPool;
    VkDescriptorPool           _descriptorPool;
    // for rasterization
    VkPipeline       _graphicsPipeline;
    VkPipelineLayout _graphicsPipelineLayout;
    struct DepthTexture
    {
        VkImage        image;
        VkDeviceMemory memory;
        VkImageView    view;
        VkImageLayout  layout;
    } _rasterizationDepthImage;
    VkFramebuffer _rasterizationFBO;
    VkRenderPass  _rasterizationRenderPass;
    // for both ray tracing and rasterization
    VkDescriptorSetLayout _descriptorSetLayout;
    VkDescriptorSet       _descriptorSet;
    // for ray tracing
    VkPipeline                                        _rtPipeline;
    VkPipelineLayout                                  _rtPipelineLayout;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> _rtShaderGroups;

    // env texture
    Texture _envTexture;

    // for post processing
    VkPipeline            _postPipeline;
    VkPipelineLayout      _postPipelineLayout;
    VkDescriptorSetLayout _postDescriptorSetLayout;
    VkDescriptorSet       _postDescriptorSet;

    Scene                       _scene;
    Texture                     _rayTraceRT;
    RenderUniform               _renderUniformData;
    Buffer                      _renderUniformBuffer;
    uint32_t                    _frameCount = 0;
    std::vector<nvvk::AccelKHR> _blasAccels;
    VkAccelerationStructureKHR  _tlasAccels;
    nvh::CameraManipulator::Camera _dirtyCamera;
    Texture                        _envLookupTexture;
};

inline Texture& PlayApp::AllocTexture()
{
    return _texturePool.alloc();
}
inline void PlayApp::FreeTexture(Texture& texture)
{
    _texturePool.free(texture);
}
inline Buffer& PlayApp::AllocBuffer()
{
    return _bufferPool.alloc();
}
inline void PlayApp::FreeBuffer(Buffer& buffer)
{
    _bufferPool.free(buffer);
}

} //    namespace Play

#endif // PLAYAPP_H