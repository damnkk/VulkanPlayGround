#include "MaterialManager.h"
#include "ShaderManager.hpp"
#include "nvutils/logger.hpp"

namespace Play
{
MaterialManager& MaterialManager::Instance()
{
    static MaterialManager manager;
    return manager;
}

bool MaterialManager::initBuiltinMaterials()
{
    auto& shaders  = ShaderManager::Instance();
    auto  material = std::make_shared<Material>(shaders.getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME),
                                                shaders.getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME),
                                                BuiltinMaterials::BUILTIN_DEFAULT_GBUFFER_MATERIAL_NAME);
    return registerMaterial(material);
}

bool MaterialManager::registerMaterial(const std::shared_ptr<Material>& material)
{
    if (!material || material->getName().empty())
    {
        LOGE("Cannot register a null or unnamed material\n");
        return false;
    }

    const auto& name = material->getName();
    if (_materials.find(name) != _materials.end())
    {
        LOGE("Material {%s} is already registered\n", name.c_str());
        return false;
    }

    _materials.emplace(name, MaterialEntry{material, {material->createMaterialInstance()}});
    return true;
}

std::shared_ptr<Material> MaterialManager::getMaterial(const std::string& materialName) const
{
    const auto iter = _materials.find(materialName);
    return iter == _materials.end() ? nullptr : iter->second.material;
}

std::shared_ptr<MaterialInstance> MaterialManager::getOrCreateMaterialInstance(const std::string&        materialName,
                                                                              const MaterialRenderState& renderState)
{
    const auto iter = _materials.find(materialName);
    if (iter == _materials.end())
    {
        return nullptr;
    }

    MaterialEntry& entry = iter->second;
    for (const auto& instance : entry.instances)
    {
        if (instance->getRenderState() == renderState)
        {
            return instance;
        }
    }

    auto instance = entry.material->createMaterialInstance();
    instance->setRenderState(renderState);
    entry.instances.push_back(instance);
    return instance;
}

void MaterialManager::deInit()
{
    _materials.clear();
}
} // namespace Play
