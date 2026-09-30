#ifndef MODEL_H
#define MODEL_H
#include "core/assets/Asset.h"
#include "memory"
#include "VPGLoader/VPGLoader.hpp"
#include "Hdevice.h"
#include "resourceManagement/vulkan/resources/Resource.h"
namespace Play
{
class MaterialInstance;

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
        return _modelDesc;
    }

    Buffer* getAssetBuffer() const
    {
        return _assetBuffer.get();
    }

    const std::vector<RefPtr<Texture>>& getTextures() const
    {
        return _textures;
    }

    const std::vector<ModelDrawableInfo>& getDrawableInfos() const
    {
        return _drawableInfos;
    }
    uint64_t getDrawableAddress() const
    {
        return _drawableAddress;
    }

    MaterialInstance* getDefaultMaterialInstance(uint32_t materialIndex) const
    {
        return _materialInstances.at(materialIndex).get();
    }

protected:
    vpgloader::ModelHandle _loadedModel;
    // CPU-side drawables and model bounds, kept after the GPU upload for
    // per-frame updates. VPGLoader v3 resolves the source node hierarchy at
    // import time, so a drawable's modelFromMesh maps its mesh-local vertices
    // straight into model space. Everything else in _loadedModel is released in
    // onLoadAsset once it has been uploaded to GPU buffers and textures.
    std::vector<vpgloader::ModelDrawable>          _drawables;
    std::vector<ModelDrawableInfo>                 _drawableInfos;
    vpgloader::AABB                                _modelBounds;
    RefPtr<Buffer>                                 _assetBuffer;
    std::vector<RefPtr<Texture>>                   _textures;
    ModelBufferInfo                                _modelDesc{};
    uint64_t                                       _drawableAddress = 0;
    std::vector<std::shared_ptr<MaterialInstance>> _materialInstances;

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
