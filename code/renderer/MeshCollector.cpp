#include "MeshCollector.h"

#include "SceneManager.h"
#include "renderer/Renderer.h"

#include <assert.h>

namespace Play
{

std::vector<MeshBatch>& MeshCollector::collectMeshBatches()
{
    assert(_renderer);
    assert(_renderer->getSceneManager());

    _meshBatches.clear();
    return _meshBatches;
}

} // namespace Play
