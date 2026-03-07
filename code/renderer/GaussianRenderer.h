#ifndef GAUSSIAN_RENDERER_H
#define GAUSSIAN_RENDERER_H
#include "renderer.h"

namespace Play
{

class GaussianRenderer : public Renderer
{
public:
    explicit GaussianRenderer(PlayElement& element);
    ~GaussianRenderer() override;
    void OnGUI() override;

protected:
    void setupPasses() override;
};

} // namespace Play

#endif // GAUSSIAN_RENDERER_H