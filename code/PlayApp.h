#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvvkhl/appbase_vk.hpp"
#include "resourceManagement/ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include "nvvk/memallocator_vma_vk.hpp"
#include "nvvk/acceleration_structures.hpp"
#include "resourceManagement/Resource.h"
#include "resourceManagement/SceneNode.h"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"
#include "nvp/NvFoundation.h"
#include "pch.h"
#include "renderer/Renderer.h"
namespace Play
{
struct Renderer;
struct RTRenderer;
struct VolumeRenderer;
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
    nvvk::ResourceAllocatorVma& getAlloc()
    {
        return _alloc;
    }
    inline Texture* CreateTexture(VkImageCreateInfo& info, VkCommandBuffer* cmd = nullptr);
    template <typename T>
    static inline Texture* AllocTexture();
    static inline void     FreeTexture(Texture* texture);
    inline Buffer* CreateBuffer(VkBufferCreateInfo& info, VkMemoryPropertyFlags memProperties);
    template <typename T>
    static inline Buffer*  AllocBuffer();
    static inline void     FreeBuffer(Buffer* buffer);
    static inline void*    MapBuffer(Buffer& buffer);
    static inline void     UnmapBuffer(Buffer& buffer);

    nvvk::DebugUtil m_debug;

   protected:
    void createGraphicsDescriptResource();
    void createGraphicsDescriptorSet();
    void createGraphicsDescriptorSetLayout();
    void createGraphicsPipeline();

   private:
    friend class RTRenderer;
    friend class VolumeRenderer;
    friend class ShadingRateRenderer;

    enum RenderMode
    {
        eRasterization,
        eRayTracing,
        eVolumeRendering,
        eShadingRateRendering,
        eDeferRendering,
        eRCount
    } _renderMode = eShadingRateRendering;
    friend class ModelLoader;
    ModelLoader _modelLoader;
    static nvvk::ResourceAllocatorVma _alloc;
    static TexturePool         _texturePool;
    static BufferPool          _bufferPool;
    VkDescriptorPool          _descriptorPool;

    VkDescriptorSet          _graphicDescriptorSet;
    VkDescriptorSetLayout    _graphicsSetLayout;
    VkPipeline                _graphicsPipeline;
    VkPipelineLayout         _graphicsPipelineLayout;
    std::unique_ptr<Renderer> _renderer;
    Scene                     _scene;
};

template <typename T>
inline Texture* PlayApp::AllocTexture()
{
    return _texturePool.alloc<T>();
}

inline void PlayApp::FreeTexture(Texture* texture)
{
    _texturePool.free(texture);
}

template <typename T>
inline Buffer* PlayApp::AllocBuffer()
{
    return _bufferPool.alloc<T>();
}

inline void PlayApp::FreeBuffer(Buffer* buffer)
{
    _bufferPool.free(buffer);
}

inline void* PlayApp::MapBuffer(Buffer& buffer)
{
    return _alloc.map(buffer);
}

inline void PlayApp::UnmapBuffer(Buffer& buffer)
{
    _alloc.unmap(buffer);
}

} //    namespace Play

#endif // PLAYAPP_H