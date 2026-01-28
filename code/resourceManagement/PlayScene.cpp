#include "PlayScene.h"
#include "Material.h"
#include <nvvkgltf/scene.hpp>
namespace Play
{
void RenderScene::fillDefaultMaterials(nvvkgltf::Scene& scene)
{
    this->_defaultMaterials.clear();
    this->_defaultMaterials.resize(scene.getModel().materials.size());
    this->_defaultMaterials.assign(scene.getModel().materials.size(), FixedMaterial::Create());
}
} // namespace Play