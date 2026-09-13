#ifndef ASSET_H
#define ASSET_H
#include "core/Serializable.h"
#include "core/Uuid.h"
namespace Play
{
class AssetManager;

enum AssetType
{
    ASSET_TYPE_UNKNOWN = 0,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_SCENE,
    ASSET_TYPE_MAX_ENUM
};

class Asset
{
public:
    explicit Asset(const std::string& name = "Unknown") : _name(name) {}
    virtual ~Asset() = default;
    virtual std::string getAssetTypeName() const
    {
        return "Unknown";
    };
    virtual AssetType getAssetType() const
    {
        return ASSET_TYPE_UNKNOWN;
    };
    virtual void onLoadAsset();
    virtual bool onSaveAsset();
    inline const VUID getUID()
    {
        return _uid;
    }
    inline const std::string& getName() const
    {
        return _name;
    }
    void setName(const std::string& name)
    {
        _name = name;
    }
    inline const std::string& getFilePath() const
    {
        return _filePath;
    }

protected:
    VUID _uid;
    friend class Play::AssetManager;

    std::string _name;
    std::string _filePath;

private:
    // clang-format off
    BeginSerailize() 
    SerailizeEntry(_name)
    SerailizeEntry(_filePath)
    SerailizeEntry(_uid) 
    EndSerailize
    // clang-format on 
};

typedef std::shared_ptr<Asset> AssetRef;

class AssetBinder{
protected:
    std::unordered_map<std::string, VUID> _assetMap;
    std::unordered_map<std::string, std::vector<VUID>> _assetArrayMap;
private:
// clang-format off
    BeginSerailize()
    SerailizeEntry(_assetMap)
    SerailizeEntry(_assetArrayMap)
    EndSerailize
    // clang-format on
};

#define BeginLoadAssetBind() {
#define LoadAssetBind(className, bind)                                             \
    do                                                                             \
    {                                                                              \
        auto iter = assetMap.find(#bind);                                          \
        if (iter != assetMap.end() && !iter->second.IsEmpty())                     \
        {                                                                          \
            bind = vkDriver->getAssetManager()->getAsset<className>(iter->second); \
        }                                                                          \
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

#define LoadAssetArrayBind(className, bind)                                                      \
    do                                                                                           \
    {                                                                                            \
        auto iter = assetArrayMap.find(#bind);                                                   \
        if (iter != assetArrayMap.end())                                                         \
        {                                                                                        \
            for (uint32_t i = 0; i < iter->second.size(); i++)                                   \
            {                                                                                    \
                if (!iter->second[i].IsEmpty())                                                  \
                {                                                                                \
                    bind[i] = vkDriver->getAssetManager()->getAsset<className>(iter->second[i]); \
                }                                                                                \
            }                                                                                    \
        }                                                                                        \
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
        assetMap.emplace(#bind, VUID{});

#define SaveAssetArrayBind(bind)                                 \
    do                                                           \
    {                                                            \
        std::vector<VUID> uids;                                  \
        for (uint32_t i = 0; i < bind.size(); i++)               \
        {                                                        \
            if (bind[i])                                         \
            {                                                    \
                vkDriver->getAssetManager()->saveAsset(bind[i]); \
                uids.push_back(bind[i]->getUID());               \
            }                                                    \
            else                                                 \
                uids.push_back(VUID{});                          \
        }                                                        \
        assetArrayMap.emplace(#bind, uids);                      \
    } while (0);

#define EndSaveAssetBind }
} // namespace Play
#endif // ASSET_H
