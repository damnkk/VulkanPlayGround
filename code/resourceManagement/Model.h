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

    const std::vector<RefPtr<Texture>>& getTextures() const
    {
        return _renderData.textures;
    }

protected:
    vpgloader::ModelHandle _loadedModel;
    // CPU-side drawables and model bounds, kept after the GPU upload for
    // per-frame updates. VPGLoader v3 resolves the source node hierarchy at
    // import time, so a drawable's modelFromMesh maps its mesh-local vertices
    // straight into model space. Everything else in _loadedModel is released in
    // onLoadAsset once it has been uploaded to GPU buffers and textures.
    std::vector<vpgloader::ModelDrawable> _drawables;
    vpgloader::AABB                       _modelBounds;
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

using ModelRef = std::shared_ptr<Model>;

} // namespace Play

#endif // MODEL_H
