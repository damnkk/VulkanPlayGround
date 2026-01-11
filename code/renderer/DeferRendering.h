#ifndef DEFERRENDERING_H
#define DEFERRENDERING_H
#include "Renderer.h"
#include <bitset>
#include "RDG/RDG.h"
namespace Play
{

enum class DeferPasses
{
    ePreDepthPass,
    eBaseColorPass,
    eLightPass,
    eVolumeSkyPass,
    ePostProcessPass,
    eCount
};
class BasePass;
class DeferRenderer : public Renderer
{
public:
    explicit DeferRenderer(PlayElement& element);
    ~DeferRenderer() override;
    void OnGUI() override;
    void OnPreRender() override;
    void OnPostRender() override;
    void RenderFrame() override;
    void SetScene(SceneManager* scene) override;
    void OnResize(int width, int height) override;

    Texture* getOutputTexture() override
    {
        return _outputTexture;
    }

protected:
    void updateCameraBuffer();

private:
    std::unique_ptr<RDG::RDGBuilder>         _rdgBuilder;
    Texture*                                 _outputTexture = nullptr;
    std::bitset<size_t(DeferPasses::eCount)> _renderPasses;
    std::vector<std::unique_ptr<BasePass>>   _passes;
    PlayElement*                             _view = nullptr;
};
} // namespace Play
#endif // DEFERRENDERING_H