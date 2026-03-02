#ifndef GAUSSIAN_RENDERER_H
#define GAUSSIAN_RENDERER_H
#include "renderer.h"
#include "RDG/RDG.h"
namespace Play
{

class BasePass;
class GaussianRenderer : public Renderer
{
public:
    explicit GaussianRenderer(PlayElement& element);
    ~GaussianRenderer() override;
    void     OnGUI() override;
    void     OnPreRender() override;
    void     RenderFrame() override;
    void     OnPostRender() override;
    void     SetScene(SceneManager* scene) override;
    void     OnResize(int width, int height) override;
    Texture* getOutputTexture() override
    {
        return _outputTexture;
    }

protected:
private:
    std::unique_ptr<RDG::RDGBuilder>       _rdgBuilder;
    Texture*                               _outputTexture = nullptr;
    std::vector<std::unique_ptr<BasePass>> _passes;
    PlayElement*                           _view = nullptr;
};

} // namespace Play

#endif // GAUSSIAN_RENDERER_H