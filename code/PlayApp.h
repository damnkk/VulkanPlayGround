#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvapp/application.hpp"
// #include "resourceManagement/ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include <nvvk/profiler_vk.hpp>
#include <nvutils/parameter_parser.hpp>
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

class PlayElement : public nvapp::IAppElement{
public:
      // Interface
    virtual void onAttach(nvapp::Application* app) override;                      // Called once at start
    virtual void onDetach() override;                                             // Called before destroying the application
    virtual void onResize(VkCommandBuffer cmd, const VkExtent2D& size) override;  // Called when the viewport size is changing
    virtual void onUIRender() override;                                           // Called for anything related to UI
    virtual void onUIMenu() override;                                             // This is the menubar to create
    virtual void onPreRender() override;                                          // called post onUIRender and prior onRender (looped over all elements)
    virtual void onRender(VkCommandBuffer cmd) override;                          // For anything to render within a frame
    virtual void onFileDrop(const std::filesystem::path& filename) override;      // For when a file is dragged on top of the window
    virtual void onLastHeadlessFrame() override;                                  // Called at the end of the last frame in headless mode
    virtual ~PlayElement();

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

    enum RenderMode
    {
        eRasterization,
        eRayTracing,
        eVolumeRendering,
        eShadingRateRendering,
        eDeferRendering,
        eRCount
    } _renderMode = eShadingRateRendering;
protected:
    //SceneManager
    //RenderPassCache
    //FrameBufferCache
    //PipelineCache
    std::unique_ptr<Renderer> _renderer;
    Texture _uiTexture;
    VkDescriptorSet _uiTextureDescriptor;

private:
    friend class RTRenderer;
    friend class VolumeRenderer;
    friend class ShadingRateRenderer;
    friend class RDG::RenderDependencyGraph;
    nvapp::Application* _app{};
    nvutils::ProfilerTimeline* m_profilerTimeline{};
    nvvk::ProfilerGpuTimer     m_profilerGpuTimer{};
};



inline Texture* PlayElement::AllocTexture()
{
    return _texturePool.alloc();
}

inline void PlayElement::FreeTexture(Texture* texture)
{
    _texturePool.free(texture);
}

inline Buffer* PlayElement::AllocBuffer()
{
    return _bufferPool.alloc();
}

inline void PlayElement::FreeBuffer(Buffer* buffer)
{
    _bufferPool.free(buffer);
}

inline void* PlayElement::MapBuffer(Buffer& buffer)
{
    return _alloc.map(buffer);
}

inline void PlayElement::UnmapBuffer(Buffer& buffer)
{
    _alloc.unmap(buffer);
}

} //    namespace Play

#endif // PLAYAPP_H