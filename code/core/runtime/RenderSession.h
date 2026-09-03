#ifndef RENDERSESSION_H
#define RENDERSESSION_H
#include "resourceManagement/vulkan/resources/Resource.h"

namespace Play
{
class Renderer;
class RTRenderer;
class VolumeRenderer;
class ShaderInfo;
class DescriptorSetCache;
class RenderPassCache;
class FrameBufferCache;

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

    RenderMode getRenderMode() const
    {
        return _renderMode;
    }

protected:
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
    bool _initialized = false;
};

} //    namespace Play

#endif // RENDERSESSION_H
