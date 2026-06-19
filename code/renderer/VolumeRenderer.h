#ifndef VOLUME_RENDERER_H
#define VOLUME_RENDERER_H

#include "Renderer.h"
#include <rttr/rttr_enable.h>

namespace Play
{
class VolumeRenderPass;

class VolumeRenderer : public Renderer
{
public:
    explicit VolumeRenderer(RenderSession& session);
    ~VolumeRenderer() override;

    void OnPreRender() override;
    void OnResize(int width, int height) override;

    RTTR_ENABLE(Renderer)

protected:
    void setupPasses() override;

private:
    VolumeRenderPass* _volumePass = nullptr;
};

} // namespace Play

#endif // VOLUME_RENDERER_H
