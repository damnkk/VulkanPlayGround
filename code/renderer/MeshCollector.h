#ifndef MESH_COLLECTOR_H
#define MESH_COLLECTOR_H
#include <cstdint>
#include <vector>
namespace Play
{
class Renderer;
class RenderScene;
// at the begining,just a small structure with single mesh info, later will be extended
struct MeshBatch
{
    uint32_t renderNodeID;
    uint32_t sceneID;
};

// one submit class, recreate per frame
class MeshCollector
{
public:
    MeshCollector(Renderer* render) : _renderer(render) {};
    ~MeshCollector() = default;
    std::vector<MeshBatch>& collectMeshBatches();

private:
    std::vector<MeshBatch> _meshBatches;
    Renderer*              _renderer = nullptr;
};
} // namespace Play

#endif // MESH_COLLECTOR_H