#include "AssetLoadingServer.h"
#include "core/JobSystem.h"
#include "core/Profiling.h"
#include "core/ProjectPaths.h"
#include "nvutils/logger.hpp"
namespace Play
{
namespace
{
std::filesystem::path makeDefaultAssetPath(const VUID& uid)
{
    if (!ProjectInfo::isOpen() || uid.is_nil())
    {
        return {};
    }

    return ProjectInfo::getProjectPath() / "asset" / (uuids::to_string(uid) + ".json");
}
} // namespace

bool AssetManager::Init()
{
    const std::filesystem::path manifestPath = ProjectInfo::getAssetMapPath();
    if (manifestPath.empty())
    {
        LOGE("Cannot initialize the asset manager without a project path\n");
        return false;
    }

    _assets.clear();
    _uninitializedAssets.clear();
    _pathToGUID.clear();
    _GUIDToPath.clear();

    std::error_code errorCode;
    const bool      manifestExists = std::filesystem::exists(manifestPath, errorCode);
    if (errorCode)
    {
        LOGE("Failed to inspect asset manifest {%s}: %s\n", manifestPath.string().c_str(), errorCode.message().c_str());
        return false;
    }

    // A missing manifest is the normal initial state of a newly created project.
    if (!manifestExists)
    {
        return true;
    }

    if (!std::filesystem::is_regular_file(manifestPath, errorCode) || errorCode)
    {
        LOGE("Asset manifest is not a readable file {%s}\n", manifestPath.string().c_str());
        return false;
    }

    std::ifstream manifestStream(manifestPath);
    if (!manifestStream.is_open())
    {
        LOGE("Failed to open asset manifest {%s}\n", manifestPath.string().c_str());
        return false;
    }

    try
    {
        cereal::JSONInputArchive archive(manifestStream);
        archive(cereal::make_nvp("assetManager", *this));
    }
    catch (const std::exception& error)
    {
        LOGE("Failed to deserialize asset manifest {%s}: %s\n", manifestPath.string().c_str(), error.what());
        return false;
    }

    return true;
}

void AssetManager::Tick() {}

void AssetManager::Save()
{
    const std::filesystem::path manifestPath = ProjectInfo::getAssetMapPath();
    if (manifestPath.empty())
    {
        LOGE("Cannot save the asset manifest without a project path\n");
        return;
    }

    std::ofstream manifestStream(manifestPath, std::ios::trunc);
    if (!manifestStream.is_open())
    {
        LOGE("Failed to open asset manifest for writing {%s}\n", manifestPath.string().c_str());
        return;
    }

    try
    {
        cereal::JSONOutputArchive archive(manifestStream);
        archive(cereal::make_nvp("assetManager", *this));
    }
    catch (const std::exception& error)
    {
        LOGE("Failed to serialize asset manifest {%s}: %s\n", manifestPath.string().c_str(), error.what());
    }
}

VUID AssetManager::filePathToGUID(const std::string& path)
{
    auto iter = _pathToGUID.find(path);
    if (iter == _pathToGUID.end())
    {
        return uuids::uuid{};
    }
    return iter->second;
}

std::string AssetManager::guidToFilePath(const VUID& uid)
{
    auto iter = _GUIDToPath.find(uid);
    if (iter == _GUIDToPath.end())
    {
        return "";
    }
    return iter->second;
}

AssetRef AssetManager::importAsset(AssetRef asset, const std::string& filePath, const std::string& assetFilePath)
{
    if (!asset)
    {
        LOGW("Cannot import null asset from {%s}\n", filePath.c_str());
        return nullptr;
    }

    if (filePath.empty())
    {
        LOGW("Fail to import asset with empty path\n");
        return nullptr;
    }

    if (asset->_uid.is_nil())
    {
        asset->_uid = createAssetGUID();
    }

    asset->_filePath = filePath;
    asset->onLoadAsset();
    _assets[asset->getUID()] = asset;
    saveAsset(asset, assetFilePath);
    return asset;
}

VUID AssetManager::createAssetGUID()
{
    static std::random_device           randomDevice;
    static std::mt19937                 randomEngine(randomDevice());
    static uuids::uuid_random_generator uuidGenerator(randomEngine);
    VUID                                uid;

    do
    {
        uid = uuidGenerator();
    } while (uid.is_nil() || _GUIDToPath.find(uid) != _GUIDToPath.end() || _assets.find(uid) != _assets.end() ||
             _uninitializedAssets.find(uid) != _uninitializedAssets.end());

    return uid;
}

AssetRef AssetManager::getAsset(const std::string& path)
{
    VUID guid = filePathToGUID(path);
    if (guid.is_nil())
    {
        LOGW("Fail to find asset from cache {%s}", path.c_str());
        return nullptr;
    }
    return getAsset(guid);
}

AssetRef AssetManager::getAsset(const VUID& uid)
{
    if (_assets.find(uid) != _assets.end()) return _assets[uid];

    auto iter = _uninitializedAssets.find(uid);
    if (iter != _uninitializedAssets.end())
    {
        AssetRef asset = iter->second;
        asset->onLoadAsset();
        _uninitializedAssets.erase(iter);
        _assets[uid] = asset;
        return asset;
    }

    LOGW("Fail to find asset from cache {%s}", uuids::to_string(uid).c_str());
    return {};
}

void AssetManager::saveAsset(AssetRef asset, const std::string& filePath)
{
    if (!asset)
    {
        LOGW("Cannot save null asset\n");
        return;
    }

    if (asset->_uid.is_nil())
    {
        asset->_uid = createAssetGUID();
    }

    std::filesystem::path path = filePath.empty() ? std::filesystem::path(guidToFilePath(asset->getUID())) : std::filesystem::path(filePath);
    if (path.empty())
    {
        path = makeDefaultAssetPath(asset->getUID());
    }

    if (path.empty())
    {
        LOGW("Cannot save asset {%s} without an open project or an explicit path\n", uuids::to_string(asset->getUID()).c_str());
        return;
    }

    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    if (errorCode)
    {
        LOGW("Failed to create asset directory {%s}: %s\n", path.parent_path().string().c_str(), errorCode.message().c_str());
        return;
    }

    asset->onSaveAsset();

    std::ofstream assetStream(path, std::ios::trunc);
    if (!assetStream.is_open())
    {
        LOGW("Failed to open asset file for writing {%s}\n", path.string().c_str());
        return;
    }

    try
    {
        cereal::JSONOutputArchive archive(assetStream);
        archive(asset);
    }
    catch (const std::exception& error)
    {
        LOGW("Failed to serialize asset {%s}: %s\n", path.string().c_str(), error.what());
        return;
    }

    updateFilePathAndGUID(path.string(), asset->getUID());
    auto uninitializedAsset = _uninitializedAssets.find(asset->getUID());
    if (uninitializedAsset != _uninitializedAssets.end())
    {
        uninitializedAsset->second = asset;
    }
    else
    {
        _assets[asset->getUID()] = asset;
    }
    Save();
}

void AssetManager::deleteAsset(AssetRef asset)
{
    if (!asset)
    {
        LOGW("Cannot delete null asset\n");
        return;
    }

    std::string oldPath = guidToFilePath(asset->getUID());
    if (oldPath.empty())
    {
        LOGW("Failed to delete asset {%s} with unknown path\n", uuids::to_string(asset->getUID()).c_str());
        return;
    }
    deleteAsset(oldPath);
}

void AssetManager::updateFilePathAndGUID(const std::string& filePath, const VUID& uid)
{
    if (filePath.empty())
    {
        auto uidIter = _GUIDToPath.find(uid);
        if (uidIter != _GUIDToPath.end())
        {
            _GUIDToPath.erase(uidIter);
            _pathToGUID.erase(uidIter->second);
        }
    }
    else
    {
        auto pathIter = _pathToGUID.find(filePath);
        if (pathIter != _pathToGUID.end() && pathIter->second != uid)
        {
            _GUIDToPath.erase(pathIter->second);
            _pathToGUID.erase(pathIter);
        }
        auto uidIter = _GUIDToPath.find(uid);
        if (uidIter != _GUIDToPath.end() && uidIter->second != filePath)
        {
            _pathToGUID.erase(uidIter->second);
        }
        _GUIDToPath[uid]      = filePath;
        _pathToGUID[filePath] = uid;
    }
}

void AssetManager::deleteAsset(const std::string& path)
{
    std::filesystem::remove(path);
    VUID id = filePathToGUID(path);
    if (!id.is_nil())
    {
        updateFilePathAndGUID("", id);
        _assets.erase(id);
        _uninitializedAssets.erase(id);
    }
}

AssetRef AssetManager::getOrLoadAssetInternal(const std::string& path)
{
    AssetRef asset = getAsset(path);
    if (asset == nullptr) asset = loadAsset(path, true);
    if (asset == nullptr)
    {
        LOGW("Fail to load asset {%s}", path.c_str());
    }
    else
    {
        LOGI("Load asset {%s}", path.c_str());
    }
    return asset;
}

AssetRef AssetManager::getOrLoadAssetInternal(const VUID& uid)
{
    AssetRef asset = getAsset(uid);
    if (asset == nullptr)
    {
        LOGW("Fail to load asset {%s}", uuids::to_string(uid).c_str());
    }
    else
    {
        LOGI("Load asset {%s}", uuids::to_string(uid).c_str());
    }
    return asset;
}

AssetRef AssetManager::loadAsset(const std::string& filePath, bool init)
{
    AssetRef              asset;
    std::filesystem::path path(filePath);
    if (path.empty())
    {
        LOGW("Fail to load asset with empty path\n");
        return nullptr;
    }
    if (!path.has_extension())
    {
        LOGW("Fail to load asset with no extension\n");
        return nullptr;
    }
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        LOGW("Fail to load asset from file {%s}\n", filePath.c_str());
        return nullptr;
    }
    try
    {
        cereal::JSONInputArchive archive(ifs);
        archive(asset);
    }
    catch (const std::exception& error)
    {
        LOGW("Fail to deserialize asset from file {%s}: %s\n", filePath.c_str(), error.what());
        return nullptr;
    }

    if (!asset)
    {
        LOGW("Fail to deserialize null asset from file {%s}\n", filePath.c_str());
        return nullptr;
    }

    const std::string oldAssetFilePath = asset->getFilePath();
    if (init)
    {
        asset->onLoadAsset();
        _assets[asset->getUID()] = asset;
    }
    this->updateFilePathAndGUID(path.string(), asset->getUID());
    return asset;
}

} // namespace Play
