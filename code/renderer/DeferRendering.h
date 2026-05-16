#ifndef DEFERRENDERING_H
#define DEFERRENDERING_H
#include "Renderer.h"
#include <bitset>

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

class DeferRenderer : public Renderer
{
public:
    explicit DeferRenderer(RenderSession& session);
    ~DeferRenderer() override;

protected:
    void setupPasses() override;

private:
    std::bitset<size_t(DeferPasses::eCount)> _renderPasses;
};
} // namespace Play
#endif // DEFERRENDERING_H
