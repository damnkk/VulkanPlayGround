#ifndef RENDERPASS_H
#define RENDERPASS_H
#include <string>
namespace Play
{
namespace RDG
{
class RDGBuilder;
}

// logic pass, like gbuffer pass, light pass, not the vulkan render pass
class BasePass
{
public:
    BasePass()                                      = default;
    virtual ~BasePass()                             = default;
    virtual void init()                             = 0;
    virtual void build(RDG::RDGBuilder* rdgBuilder) = 0;
    std::string  _name;

private:
};
} // namespace Play
#endif // RENDERPASS_H