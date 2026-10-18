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

    std::shared_ptr<Material> getMaterial(const std::string& materialName) const;

    // Returns the shared instance of the named material whose render state equals
    // renderState, creating one the first time that state is requested. Sharing
    // keeps draw buckets and pipelines keyed by a small set of states.
    std::shared_ptr<MaterialInstance> getOrCreateMaterialInstance(const std::string& materialName, const MaterialRenderState& renderState);

private:
    MaterialManager()                                  = default;
    MaterialManager(const MaterialManager&)            = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    struct MaterialEntry
    {
        // Members are destroyed in reverse order, so the instances are released
        // before their source material. The first instance carries the default
        // render state.
        std::shared_ptr<Material>                      material;
        std::vector<std::shared_ptr<MaterialInstance>> instances;
    };

    std::unordered_map<std::string, MaterialEntry> _materials;
};
} // namespace Play

#endif // MATERIALMANAGER_H
