#ifndef GAUSSIAN_RENDERER_H
#define GAUSSIAN_RENDERER_H
#include "renderer.h"
#include <rttr/rttr_enable.h>

namespace Play
{

class GaussianRenderer : public Renderer
{
public:
    explicit GaussianRenderer(RenderSession& session);
    ~GaussianRenderer() override;

    RTTR_ENABLE(Renderer)

protected:
    void setupPasses() override;
};

} // namespace Play

#endif // GAUSSIAN_RENDERER_H
