#ifndef PLAY_PRESENTPASS_H
#define PLAY_PRESENTPASS_H

#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory>
namespace Play
{
class Renderer;
class RenderProgram;

class PresentPass : public BasePass
{
public:
    PresentPass(Renderer* renderer) : _renderer(renderer) {}
    static constexpr const char* PRESENT_TEXTURE_NAME = "presentTexture";

    ~PresentPass();

    void init() override;
    // build 需要接收一个输入纹理，即上一阶段的输出
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    Renderer*             _renderer;
    RefPtr<RenderProgram> _presentProgram;
};

} // namespace Play

#endif // PLAY_PRESENTPASS_H
