#ifndef MATERIALMANAGER_H
#define MATERIALMANAGER_H

#include "Material.h"

namespace Play
{
namespace BuiltinMaterials
{
inline const std::string BUILTIN_DEFAULT_GBUFFER_MATERIAL_NAME = "defaultgbufferMaterial";
} // namespace BuiltinMaterials

class MaterialManager
{
public:
    static MaterialManager& Instance();

    // Called after ShaderManager initialization and before project loading.
    bool initBuiltinMaterials();
    void deInit();

    // Registration creates one default instance. Material names are registry keys
    // and must remain unchanged while registered; duplicate names are rejected.
    bool registerMaterial(const std::shared_ptr<Material>& material);

    std::shared_ptr<Material>         getMaterial(const std::string& materialName) const;
    std::shared_ptr<MaterialInstance> getDefaultMaterialInstance(const std::string& materialName) const;

private:
    MaterialManager()                                  = default;
    MaterialManager(const MaterialManager&)            = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    struct MaterialEntry
    {
        // Members are destroyed in reverse order, so the default instance is
        // released before its source material.
        std::shared_ptr<Material>         material;
        std::shared_ptr<MaterialInstance> defaultInstance;
    };

    std::unordered_map<std::string, MaterialEntry> _materials;
};
} // namespace Play

#endif // MATERIALMANAGER_H
