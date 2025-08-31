#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvapp/application.hpp"
// #include "resourceManagement/ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include <nvvk/profiler_vk.hpp>
#include <nvvk/context.hpp>
#include <nvutils/parameter_parser.hpp>
#include "PlayAllocator.h"
#include "resourceManagement/Resource.h"
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
    struct Info
    {
        nvutils::ProfilerManager*   profilerManager{};
        nvutils::ParameterRegistry* parameterRegistry{};
    };
      // Interface
    PlayElement(Info info):_info(info){}
    virtual ~PlayElement() {};
    virtual void onAttach(nvapp::Application* app) override;                      // Called once at start
    virtual void onDetach() override;                                             // Called before destroying the application
    virtual void onResize(VkCommandBuffer cmd, const VkExtent2D& size) override;  // Called when the viewport size is changing
    virtual void onUIRender() override;                                           // Called for anything related to UI
    virtual void onUIMenu() override;                                             // This is the menubar to create
    virtual void onPreRender() override;                                          // called post onUIRender and prior onRender (looped over all elements)
    virtual void onRender(VkCommandBuffer cmd) override;                          // For anything to render within a frame
    virtual void onFileDrop(const std::filesystem::path& filename) override;      // For when a file is dragged on top of the window
    virtual void onLastHeadlessFrame() override;                                  // Called at the end of the last frame in headless mode
    
    nvapp::Application* getApp(){return _app;}
    VkDevice getDevice(){return _app->getDevice();}


    inline Texture* CreateTexture(VkImageCreateInfo& info, VkCommandBuffer* cmd = nullptr);
    static inline Texture* AllocTexture();
    static inline void     FreeTexture(Texture* texture);
    inline Buffer* CreateBuffer(VkBufferCreateInfo& info, VkMemoryPropertyFlags memProperties);
    static inline Buffer*  AllocBuffer();
    static inline void     FreeBuffer(Buffer* buffer);
    static inline void*    MapBuffer(Buffer& buffer);
    static inline void     UnmapBuffer(Buffer& buffer);

    

    enum RenderMode
    {
        eRasterization,
        eRayTracing,
        eVolumeRendering,
        eShadingRateRendering,
        eDeferRendering,
        eRCount
    } _renderMode = eDeferRendering;
protected:
    //SceneManager
    //RenderPassCache
    //FrameBufferCache
    //PipelineCache
    std::shared_ptr<Renderer> _renderer;
    Texture* _uiTexture;
    VkDescriptorSet _uiTextureDescriptor;
    VkDescriptorPool _descriptorPool;
    void createGraphicsDescriptResource();

private:

    Info _info;
    friend class RTRenderer;
    friend class VolumeRenderer;
    friend class ShadingRateRenderer;
    friend class RDG::RenderDependencyGraph;
    nvapp::Application* _app{};
    nvutils::ProfilerTimeline* m_profilerTimeline{};
    nvvk::ProfilerGpuTimer     m_profilerGpuTimer{};
};

std::filesystem::path getBaseFilePath();

} //    namespace Play

#endif // PLAYAPP_H