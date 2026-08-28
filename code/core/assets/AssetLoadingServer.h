#ifndef ASSET_LOADING_SERVER_H
#define ASSET_LOADING_SERVER_H
#include "core/Uuid.h"
#include "Asset.h"
#include "core/Serializable.h"
#include <cereal/types/unordered_map.hpp>
namespace Play
{

class AssetManager
{
public:
    AssetManager() = default;
    ~AssetManager() {};
    bool Init();
    void Tick();
    void Save();

    GUID        filePathToGUID(const std::string& filePath);
    std::string guidToFilePath(const GUID& guid);

    template <typename Type>
    std::shared_ptr<Type> getOrLoadAsset(const std::string& filePath)
    {
        AssetRef asset = getOrLoadAssetInternal(filePath);
        return std::dynamic_pointer_cast<Type>(asset);
    }

    template <typename Type>
    std::shared_ptr<Type> getOrLoadAsset(const GUID& guid)
    {
        AssetRef asset = getOrLoadAssetInternal(guid);
        return std::dynamic_pointer_cast<Type>(guid);
    }

    AssetRef getAsset(const std::string& filePath);
    AssetRef getAsset(const GUID& guid);

    void saveAsset(AssetRef asset, const std::string& filePath = "");
    void deleteAsset(AssetRef asset);
    void deleteAsset(const std::string& filePath);

    const std::unordered_map<GUID, AssetRef>& getAssets() const
    {
        return _assets;
    }

protected:
private:
    std::unordered_map<GUID, AssetRef>    _assets;
    std::unordered_map<GUID, AssetRef>    _uninitializedAssets;
    std::unordered_map<std::string, GUID> _pathToGUID;
    std::unordered_map<GUID, std::string> _GUIDToPath;

    void     updateFilePathAndGUID(const std::string& filePath, const GUID& uid);
    AssetRef getOrLoadAssetInternal(const std::string& filePath);
    AssetRef getOrLoadAssetInternal(const GUID& guid);
    AssetRef loadAsset(const std::string& path, bool init = false);

private:
    BeginSerailize()
    SerailizeEntry(_GUIDToPath)
    if constexpr (Archive::is_loading::value)
    {
        _pathToGUID.clear();
        for (auto& [guid, path] : _GUIDToPath)
        {
            _pathToGUID[path] = guid;
        }
    }
    EndSerailize
};
} // namespace Play

#endif // ASSET_LOADING_SERVER_H
