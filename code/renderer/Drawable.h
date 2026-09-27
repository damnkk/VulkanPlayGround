#ifndef DRAWABLE_H
#define DRAWABLE_H
#include <cstdint>
#include <vector>
namespace Play
{
class Renderer;
class MaterialInstance;

struct DrawCommand
{
    uint32_t          drawID              = ~0U;
    uint32_t          modelID             = ~0U;
    uint32_t          drawableID          = ~0U;
    MaterialInstance* materialInstanceRef = nullptr;
};
struct DrawBatch
{
    MaterialInstance*        materialInstanceRef = nullptr;
    std::vector<DrawCommand> commands;
};
struct Drawable
{
    virtual void          collectDrawBatch(std::vector<DrawBatch>& batcheds) = 0;
    virtual void          collectAccelerationStructureInstance() {};
    virtual void          collectSurfaceCacheTask() {};
    uint32_t              materialID = 0;
    std::vector<uint32_t> drawItemIDs;
};

} // namespace Play

#endif // DRAWABLE_H
