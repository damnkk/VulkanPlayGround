#ifndef TRANSIMISSIONPASS_H
#define TRANSIMISSIONPASS_H

#include "RenderPass.h"

namespace Play
{
class DeferRenderer;

class TransimissionPass : public BasePass
{
public:
    explicit TransimissionPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    void init() override;
    void build(RDG::RDGBuilder* rdgBuilder) override;

private:
    DeferRenderer* _ownedRender = nullptr;
};

} // namespace Play

#endif // TRANSIMISSIONPASS_H
