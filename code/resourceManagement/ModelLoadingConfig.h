#ifndef MODEL_LOADING_CONFIG_H
#define MODEL_LOADING_CONFIG_H

#include "pch.h"

namespace Play
{

enum class ModelFileFormat : uint32_t
{
    eAuto,
    eGltf,
    eObj
};

struct ModelLoadingConfig
{
    static uint32_t DefaultAssimpPostProcessFlags();
    static constexpr uint32_t kAutoTextureMipLevels = 0;

    ModelFileFormat format                          = ModelFileFormat::eAuto;
    uint32_t        assimpPostProcessFlags          = DefaultAssimpPostProcessFlags();
    uint32_t        extraAssimpProcessFlags         = 0;
    float           globalScale                     = 1.0f;
    bool            loadMaterials                   = true;
    bool            loadTextures                    = true;
    bool            registerEmbeddedTexturePlaceholders = true;
    bool            srgbBaseColorTextures           = true;
    bool            srgbEmissiveTextures            = true;
    uint32_t        textureMipLevels                = kAutoTextureMipLevels;
};

struct ModelLoadRequestID
{
    uint32_t index      = ~0u;
    uint32_t generation = 0;


    bool isValid() const
    {
        return index != ~0u;
    }
};

} // namespace Play

#endif // MODEL_LOADING_CONFIG_H
