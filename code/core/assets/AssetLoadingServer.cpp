#include "AssetLoadingServer.h"
#include "core/JobSystem.h"
#include "core/Profiling.h"
#include "core/ProjectPaths.h"
#include "nvutils/logger.hpp"
namespace Play
{

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

GUID AssetManager::filePathToGUID(const std::string& path)
{
    auto iter = _pathToGUID.find(path);
    if (iter == _pathToGUID.end())
    {
        return uuids::uuid{};
    }
    return iter->second;
}

std::string AssetManager::guidToFilePath(const GUID& uid)
{
    auto iter = _GUIDToPath.find(uid);
    if (iter == _GUIDToPath.end())
    {
        return "";
    }
    return iter->second;
}

AssetRef AssetManager::getAsset(const std::string& path)
{
    GUID guid = filePathToGUID(path);
    if (guid.is_nil())
    {
        LOGW("Fail to find asset from cache {%s}", path.c_str());
        return nullptr;
    }
    return getAsset(guid);
}

AssetRef AssetManager::getAsset(const GUID& uid)
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
    std::filesystem::path path(filePath);
}

void AssetManager::deleteAsset(AssetRef asset)
{
    std::string oldPath = guidToFilePath(asset->getUID());
    if (oldPath.empty())
    {
        LOGW("Failed to delete asset {%s} with unknown path\n", uuids::to_string(asset->getUID()).c_str());
        return;
    }
    deleteAsset(oldPath);
}

void AssetManager::updateFilePathAndGUID(const std::string& filePath, const GUID& uid)
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
    GUID id = filePathToGUID(path);
    if (!id.is_nil())
    {
        updateFilePathAndGUID("", id);
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

AssetRef AssetManager::getOrLoadAssetInternal(const GUID& uid)
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
    cereal::JSONInputArchive archive(ifs);
    archive(asset);

    if (init)
    {
        asset->onLoadAsset();
        _assets[asset->getUID()] = asset;
    }
    this->updateFilePathAndGUID(path.string(), asset->getUID());
    return asset;
}

} // namespace Play
