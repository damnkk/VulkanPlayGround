#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvvkhl/appbase_vk.hpp"
#include "resourceManagement/ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include "nvvk/memallocator_vma_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "PlayAllocator.h"
#include "resourceManagement/Resource.h"
#include "resourceManagement/SceneNode.h"
#include "renderer/Renderer.h"
#include "utils.hpp"
namespace Play
{
class Renderer;
class RTRenderer;
class VolumeRenderer;
class ShaderInfo;
namespace RDG{
class RenderDependencyGraph;
}

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
    PlayAllocator& getAlloc()
    {
        return _alloc;
    }
    inline Texture* CreateTexture(VkImageCreateInfo& info, VkCommandBuffer* cmd = nullptr);
    static inline Texture* AllocTexture();
    static inline void     FreeTexture(Texture* texture);
    inline Buffer* CreateBuffer(VkBufferCreateInfo& info, VkMemoryPropertyFlags memProperties);
    static inline Buffer*  AllocBuffer();
    static inline void     FreeBuffer(Buffer* buffer);
    static inline void*    MapBuffer(Buffer& buffer);
    static inline void     UnmapBuffer(Buffer& buffer);
    static PlayAllocator   _alloc;
    static TexturePool         _texturePool;
    static BufferPool          _bufferPool;
    nvvk::DebugUtil m_debug;
    VkRenderPass GetOrCreateRenderPass(std::vector<RTState>& rtStates);
    VkPipeline GetOrCreatePipeline(nvvk::GraphicsPipelineState& pipelineState,VkPipelineLayout pipLayout,VkRenderPass targetRdPass);
    VkPipeline GetOrCreatePipeline(const ShaderInfo* computeShaderInfo);


    protected:
        void createGraphicsDescriptResource();
        void createGraphicsDescriptorSet();
        void createGraphicsDescriptorSetLayout();
        void createGraphicsPipeline();

    private:
        friend class RTRenderer;
        friend class VolumeRenderer;
        friend class ShadingRateRenderer;
        friend class RDG::RenderDependencyGraph;


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
    VkDescriptorPool          _descriptorPool;

    VkDescriptorSet          _graphicDescriptorSet;
    VkDescriptorSetLayout    _graphicsSetLayout;
    VkPipeline                _graphicsPipeline;
    VkPipelineLayout         _graphicsPipelineLayout;
    std::unique_ptr<Renderer> _renderer;
    std::unordered_map<std::size_t,VkRenderPass> _renderPassesCache;
    std::unordered_map<std::size_t,VkPipeline> _pipelineCache;
    Scene                     _scene;
};


inline Texture* PlayApp::AllocTexture()
{
    return _texturePool.alloc();
}

inline void PlayApp::FreeTexture(Texture* texture)
{
    _texturePool.free(texture);
}

inline Buffer* PlayApp::AllocBuffer()
{
    return _bufferPool.alloc();
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