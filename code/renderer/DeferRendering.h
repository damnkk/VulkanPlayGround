#ifndef DEFERRENDERING_H
#define DEFERRENDERING_H
#include "Renderer.h"
#include <bitset>
namespace Play
{

enum class DeferPasses
{
    ePreDepthPass,
    eBasePass,
    eLightPass,
    eVolumeSkyPass,
    ePostProcessPass,
    eCount
};

class DeferRenderer : public Renderer
{
   public:
    explicit DeferRenderer(PlayElement& element);
    ~DeferRenderer() override;
    void     OnPreRender() override;
    void     OnPostRender() override;
    void     RenderFrame() override;
    void     SetScene(Scene* scene) override;
    void     OnResize(int width, int height) override;
    void     OnDestroy() override;
    Texture* getOutputTexture() override
    {
        return _outputTexture;
    }

    Texture*                                 _outputTexture = nullptr;
    std::bitset<size_t(DeferPasses::eCount)> _renderPasses;
};
} // namespace Play
#endif // DEFERRENDERING_H