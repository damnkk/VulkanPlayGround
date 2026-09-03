#ifndef MODEL_H
#define MODEL_H
#include "core/assets/Asset.h"
#include "memory"
#include "VPGLoader/VPGLoader.hpp"
#include "resourceManagement/vulkan/resources/Resource.h"
namespace Play
{

class Model : public Asset, public AssetBinder, public std::enable_shared_from_this<Mesh>
{
public:
    std::string getAssetTypeName() final
    {
        return "Model";
    }

    AssetType getAssetType() final
    {
        return ASSET_TYPE_MODEL;
    }
    void onLoadAsset() override;
    void onSaveAsset() override;

protected:
    vpgloader::ModelHandle _loadedModel;
    struct RenderData
    {
        RefPtr<Buffer>               assetBuffer;
        std::vector<RefPtr<Texture>> textures;
        struct ModelDesc
        {
            uint64_t positionAddress;
            uint64_t normalAddress;
            uint64_t tangentsAddress;
            uint64_t texCoords0Address;
            uint64_t texcoords1Address;
            uint64_t colorsAddress;
            uint64_t indicesAddress;
            uint64_t meshInfoAddress;
            uint64_t builtinMaterialAddress;
            uint64_t textureInfoAddress;
            uint64_t lodOffset;
            uint32_t textureOffset;

        } modelDesc;
    } _renderData;

private:
};

} // namespace Play

#endif // MODEL_H