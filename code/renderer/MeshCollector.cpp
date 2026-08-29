#include "MeshCollector.h"

#include "resourceManagement/scene/SceneManager.h"
#include "renderer/Renderer.h"
#include "core/runtime/VulkanRuntime.h"

#include <assert.h>

namespace Play
{

std::vector<MeshBatch>& MeshCollector::collectMeshBatches()
{
    assert(_renderer);
    assert(vkDriver->getSceneManager());

    _meshBatches.clear();
    return _meshBatches;
}

} // namespace Play
