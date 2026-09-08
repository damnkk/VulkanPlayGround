#ifndef MODEL_H
#define MODEL_H
#include "core/assets/Asset.h"
#include "memory"
#include "VPGLoader/VPGLoader.hpp"
#include "Hdevice.h"
#include "resourceManagement/vulkan/resources/Resource.h"
namespace Play
{

class Model : public Asset, public AssetBinder, public std::enable_shared_from_this<Model>
{
public:
    Model() : Asset("Model") {}

    std::string getAssetTypeName() const final
    {
        return "Model";
    }

    AssetType getAssetType() const final
    {
        return ASSET_TYPE_MODEL;
    }
    void onLoadAsset() override;

    const ModelBufferInfo& getBufferInfo() const
    {
        return _renderData.modelDesc;
    }

    Buffer* getAssetBuffer() const
    {
        return _renderData.assetBuffer.get();
    }

protected:
    vpgloader::ModelHandle _loadedModel;
    struct RenderData
    {
        RefPtr<Buffer>               assetBuffer;
        std::vector<RefPtr<Texture>> textures;
        ModelBufferInfo              modelDesc{};
    } _renderData;

private:
    // clang-format off
    BeginSerailize()
    SerailizeBaseClass(Asset)
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // MODEL_H
