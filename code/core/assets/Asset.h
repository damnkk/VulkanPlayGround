#ifndef ASSET_H
#define ASSET_H
#include "core/Serializable.h"
#include "core/Uuid.h"
#include "core/runtime/VulkanRuntime.h"
enum AssetType
{
    ASSET_TYPE_UNKNOWN = 0,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_SCENE,
    ASSET_TYPE_MAX_ENUM
};

class AssetMananger;

class Asset
{
public:
    Asset()          = default;
    virtual ~Asset() = default;
    virtual std::string getAssetTypeName()
    {
        return "Unknown";
    };
    virtual AssetType getAssetType()
    {
        return ASSET_TYPE_UNKNOWN;
    };
    virtual void onLoadAsset() {

    };
    virtual void      onSaveAsset() {};
    inline const GUID getUID()
    {
        return _uid;
    }

protected:
    GUID _uid;
    friend class AssetManager;

    std::string _filePath;

private:
    // clang-format off
    BeginSerailize() 
    SerailizeEntry(_filePath)
    SerailizeEntry(_uid) 
    EndSerailize
    // clang-format on 
};

typedef std::shared_ptr<Asset> AssetRef;

class AssetBinder{
protected:
    std::unordered_map<std::string, GUID> _assetMap;
    std::unordered_map<std::string, std::vector<GUID>> _assetArrayMap;
private:
// clang-format off
    BeginSerailize()
    SerailizeEntry(_assetMap)
    SerailizeEntry(_assetArrayMap)
    EndSerailize
    // clang-format on
};

#define BeginLoadAssetBind() {
#define LoadAssetBind(className, bind)                                                   \
    do                                                                                   \
    {                                                                                    \
        auto iter = assetMap.find(#bind);                                                \
        if (iter != assetMap.end() && !iter->second.IsEmpty())                           \
        {                                                                                \
            bind = vkDriver->getAssetManager()->getOrLoadAsset<className>(iter->second); \
        }                                                                                \
    } while (0);
#define ResizeAssetArray(bind)                 \
    do                                         \
    {                                          \
        auto iter = assetArrayMap.find(#bind); \
        if (iter != assetArrayMap.end())       \
        {                                      \
            bind.resize(iter->second.size());  \
        }                                      \
    } while (0);

#define LoadAssetArrayBind(className, bind)                                                            \
    do                                                                                                 \
    {                                                                                                  \
        auto iter = assetArrayMap.find(#bind);                                                         \
        if (iter != assetArrayMap.end())                                                               \
        {                                                                                              \
            for (uint32_t i = 0; i < iter->second.size(); i++)                                         \
            {                                                                                          \
                if (!iter->second[i].IsEmpty())                                                        \
                {                                                                                      \
                    bind[i] = vkDriver->getAssetManager()->getOrLoadAsset<className>(iter->second[i]); \
                }                                                                                      \
            }                                                                                          \
        }                                                                                              \
    } while (0);
#define EndLoadAssetBind }

#define BeginSaveAssetBind() \
    {                        \
        assetMap.clear();    \
        assetArrayMap.clear();

#define SaveAssetBind(bind)                           \
    if (bind)                                         \
    {                                                 \
        vkDriver->getAssetManager()->saveAsset(bind); \
        assetMap.emplace(#bind, bind->getUID());      \
    }                                                 \
    else                                              \
        assetMap.emplace(#bind, GUID{});

#define SaveAssetArrayBind(bind)                                 \
    do                                                           \
    {                                                            \
        std::vector<GUID> uids;                                  \
        for (uint32_t i = 0; i < bind.size(); i++)               \
        {                                                        \
            if (bind[i])                                         \
            {                                                    \
                vkDriver->getAssetManager()->SaveAsset(bind[i]); \
                uids.push_back(bind[i]->getUID());               \
            }                                                    \
            else                                                 \
                uids.push_back(GUID{});                          \
        }                                                        \
        assetArrayMap.emplace(#bind, uids);                      \
    } while (0);

#define EndSaveAssetBind }
#endif // ASSET_H