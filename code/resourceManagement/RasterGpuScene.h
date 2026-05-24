#ifndef RASTER_GPU_SCENE_H
#define RASTER_GPU_SCENE_H

#include "SceneAssets.h"

namespace Play
{

struct RasterGpuTransform
{
    glm::mat4 objectToWorld     = glm::mat4(1.0f);
    glm::mat4 worldToObject     = glm::mat4(1.0f);
    glm::mat4 prevObjectToWorld = glm::mat4(1.0f);
};

using RasterGpuMaterial = shaderio::GltfShadeMaterial;

struct RasterGpuSubmesh
{
    uint32_t  firstIndex = 0;
    uint32_t  indexCount = 0;
    uint32_t  vertexBase = 0;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
};

struct RasterGpuDrawItem
{
    uint32_t transformID = INVALID_SCENE_ID;
    uint32_t submeshID   = INVALID_SCENE_ID;
    uint32_t materialID  = INVALID_SCENE_ID;
    uint32_t nodeID      = INVALID_SCENE_ID;
};

struct RasterGpuSceneHeader
{
    uint64_t transformsAddress   = 0;
    uint64_t materialsAddress    = 0;
    uint64_t textureInfosAddress = 0;
    uint64_t submeshesAddress    = 0;
    uint64_t drawItemsAddress    = 0;
    uint32_t drawCount           = 0;
    uint32_t materialCount       = 0;
    uint32_t textureInfoCount    = 0;
    uint32_t submeshCount        = 0;
    uint32_t transformCount      = 0;
};

class RasterGpuScene
{
public:
    void clear();
    void rebuild(const CpuScene& scene, AssetRegistry& assets);

    uint32_t ensureBindlessTexture(TextureAssetID textureID, AssetRegistry& assets);

    uint64_t getSourceSceneRevision() const
    {
        return _sourceSceneRevision;
    }

    const std::vector<RasterGpuTransform>& getTransforms() const
    {
        return _transforms;
    }

    const std::vector<RasterGpuMaterial>& getMaterials() const
    {
        return _materials;
    }

    const std::vector<shaderio::GltfTextureInfo>& getTextureInfos() const
    {
        return _textureInfos;
    }

    const std::vector<RasterGpuSubmesh>& getSubmeshes() const
    {
        return _submeshes;
    }

    const std::vector<RasterGpuDrawItem>& getDrawItems() const
    {
        return _drawItems;
    }

    const std::vector<RefPtr<Texture>>& getBindlessTextures() const
    {
        return _bindlessTextures;
    }

    const RasterGpuSceneHeader& getHeader() const
    {
        return _header;
    }

private:
    uint32_t appendMaterial(MaterialAssetID materialID, AssetRegistry& assets);
    uint16_t appendTextureInfo(const MaterialTextureSlot& slot, AssetRegistry& assets);
    uint32_t appendSubmesh(const ModelSubmeshAsset& submesh);

    std::vector<RasterGpuTransform>       _transforms;
    std::vector<RasterGpuMaterial>        _materials;
    std::vector<shaderio::GltfTextureInfo> _textureInfos;
    std::vector<RasterGpuSubmesh>         _submeshes;
    std::vector<RasterGpuDrawItem>        _drawItems;
    std::vector<RefPtr<Texture>>          _bindlessTextures;
    RasterGpuSceneHeader                  _header;
    uint64_t                              _sourceSceneRevision = 0;
};

} // namespace Play

#endif // RASTER_GPU_SCENE_H
