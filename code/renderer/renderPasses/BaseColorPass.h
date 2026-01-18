#ifndef BASECOLORPASS_H
#define BASECOLORPASS_H
#include "RenderPass.h"
namespace Play
{
class DeferRenderer;

class BaseColorPass :public BasePass{
public:
    BaseColorPass() = default;
    BaseColorPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~BaseColorPass() = default;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    DeferRenderer*                 _ownedRender = nullptr;
};
} // namespace Play

#endif // BASECOLORPASS_H