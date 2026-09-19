#ifndef MESH_COLLECTOR_H
#define MESH_COLLECTOR_H
#include <cstdint>
#include <vector>
namespace Play
{
class Renderer;
struct DrawBatch
{
};
struct Drawable
{
    virtual void          collectDrawBatch(std::vector<DrawBatch>& batcheds) = 0;
    virtual void          collectAccelerationStructureInstance() {};
    virtual void          collectSurfaceCacheTask() {};
    uint32_t              materialID = 0;
    std::vector<uint32_t> drawItemIDs;
};

// one submit class, recreate per frame
class MeshCollector
{
public:
    MeshCollector(Renderer* render) : _renderer(render) {};
    ~MeshCollector() = default;
    std::vector<Drawable>& collectMeshBatches();

private:
    std::vector<Drawable> _meshBatches;
    Renderer*             _renderer = nullptr;
};
} // namespace Play

#endif // MESH_COLLECTOR_H
