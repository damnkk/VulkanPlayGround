#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "resourceManagement/Resource.h"

namespace Play
{
class Renderer;
class RTRenderer;
class VolumeRenderer;
class ShaderInfo;
class DescriptorSetCache;
class RenderPassCache;
class FrameBufferCache;
class SceneManager;

class RenderSession
{
public:
    struct Info
    {
        std::string renderMode = "defer";
    };
    RenderSession(Info info);
    ~RenderSession();

    bool init();
    void destroy();
    void onResize(const VkExtent2D& size);
    void beginFrame();
    void renderFrame();

    enum RenderMode
    {
        eRasterization,
        eRayTracing,
        eVolumeRendering,
        eShadingRateRendering,
        eDeferRendering,
        eGaussianRendering,
        eRCount
    } _renderMode = eGaussianRendering;

protected:
    // SceneManager
    // RenderPassCache
    // FrameBufferCache
    // PipelineCache
    std::unique_ptr<Renderer> _renderer;

private:
    Info _info;
    friend class RTRenderer;
    friend class VolumeRenderer;
    friend class ShadingRateRenderer;
    friend class GaussianRenderer;
    friend class SceneManager;
    bool _initialized = false;
};

} //    namespace Play

#endif // PLAYAPP_H
