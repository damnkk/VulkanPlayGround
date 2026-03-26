#ifndef PLAY_PRESENTPASS_H
#define PLAY_PRESENTPASS_H

#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory>
namespace Play
{
class PlayElement;
class RenderProgram;

class PresentPass : public BasePass
{
public:
    PresentPass(PlayElement* element) : _element(element) {}
    ~PresentPass();

    void init() override;
    // build 需要接收一个输入纹理，即上一阶段的输出
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    PlayElement*          _element;
    RefPtr<RenderProgram> _presentProgram;
};

} // namespace Play

#endif // PLAY_PRESENTPASS_H