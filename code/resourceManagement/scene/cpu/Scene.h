#ifndef SCENE_H
#define SCENE_H
#include <memory>
#include "core/assets/Asset.h"
#include "nvutils/id_pool.hpp"
#include "resourceManagement/scene/cpu/Entity.h"
namespace Play
{

class Scene : public Asset, public std::enable_shared_from_this<Scene>
{
public:
    Scene();
    explicit Scene(std::string name);
    ~Scene()
    {
        _idPool.destroyAll();
    }
    virtual std::string getAssetTypeName() const override
    {
        return "Scene";
    }
    virtual AssetType getAssetType() const override
    {
        return ASSET_TYPE_SCENE;
    }
    virtual void onLoadAsset() override;
    bool         onSaveAsset(const std::string& assetFilePath) override;

    void tick(float deltaTime);

    const std::vector<std::shared_ptr<Entity>>& getEntities() const
    {
        return _entities;
    }
    std::shared_ptr<Entity>       getEntity(uint32_t id);
    std::shared_ptr<const Entity> getEntity(uint32_t id) const;
    std::shared_ptr<Entity>       getEntity(std::string name);
    std::shared_ptr<Entity>       createEntity(std::string name = {}, std::shared_ptr<Entity> parent = {});
    std::shared_ptr<Entity>       getRoot() const
    {
        return _root;
    }
    bool addEntity(std::shared_ptr<Entity> entity);

    // Removes the entity and its descendants from this scene.
    std::shared_ptr<Entity> removeEntity(std::string name);
    std::shared_ptr<Entity> removeEntity(uint32_t id);

    template <typename TComponent>
    std::vector<std::shared_ptr<TComponent>> getComponents()
    {
        std::vector<std::shared_ptr<TComponent>> components;
        for (auto& entity : _entities)
        {
            std::shared_ptr<TComponent> component = entity->TryGetComponent<TComponent>();
            if (component) components.push_back(component);
        }
        return components;
    }

protected:
    std::shared_ptr<Entity>              _root;
    std::vector<std::shared_ptr<Entity>> _entities;
    nvutils::IDPool                      _idPool{UINT32_MAX};

private:
    void registerSubtree(const std::shared_ptr<Entity>& entity, const std::shared_ptr<Entity>& parent);

    // clang-format off
    BeginSerailize()
    SerailizeBaseClass(Asset)
    SerailizeEntry(_root)
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // SCENE_H
