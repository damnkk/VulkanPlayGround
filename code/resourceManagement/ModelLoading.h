#ifndef MODEL_LOADING_H
#define MODEL_LOADING_H

#include "SceneAssets.h"

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

    ModelFileFormat format                  = ModelFileFormat::eAuto;
    uint32_t        assimpPostProcessFlags  = DefaultAssimpPostProcessFlags();
    uint32_t        extraAssimpProcessFlags = 0;
    float           globalScale             = 1.0f;
    bool            loadMaterials           = true;
    bool            loadTextures            = true;
    bool            registerEmbeddedTexturePlaceholders = true;
    bool            srgbBaseColorTextures   = true;
    bool            srgbEmissiveTextures    = true;
    uint32_t        textureMipLevels        = 1;
};

struct ModelLoadResult
{
    bool           success = false;
    ModelAssetID   model;
    CpuSceneNodeID rootNode;
    std::string    message;
};

} // namespace Play

#endif // MODEL_LOADING_H
