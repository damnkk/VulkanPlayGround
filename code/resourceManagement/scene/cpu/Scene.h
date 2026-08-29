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
    Scene() = default;
    Scene(std::string name) : _name(name) {}
    ~Scene() {};
    virtual std::string getAssetTypeName() override
    {
        return "Scene";
    }
    virtual AssetType getAssetType() override
    {
        return ASSET_TYPE_SCENE;
    }

    virtual void onLoadAsset() override;
    virtual void onSaveAsset() override;

    void tick(float deltaTime);

    std::vector<std::shared_ptr<Entity>> getEntities()
    {
        return _entities;
    }
    std::shared_ptr<Entity> getEntity(uint32_t id);
    std::shared_ptr<Entity> getEntity(std::string name);
    std::shared_ptr<Entity> createEntity(std::string name);
    bool                    addEntity(std::shared_ptr<Entity> entity);

    std::shared_ptr<Entity> removeEntity(std::string name);
    std::shared_ptr<Entity> removeEntity(uint32_t id);

    std::string getName()
    {
        return _name;
    }
    void setName(std::string name)
    {
        _name = name;
    }

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
    std::string                          _name;
    std::vector<std::shared_ptr<Entity>> _entities;
    nvutils::IDPool                      _idPool{UINT32_MAX};

private:
    // clang-format off
    BeginSerailize()
    SerailizeBaseClass(Asset)
    SerailizeEntry(_name)
    SerailizeEntry(_entities)
    for (auto& entity : _entities){
        entity->_scene = weak_from_this();
        _idPool.createID(entity->_id);
    } 
    SerailizeEntry(_idPool)
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // SCENE_H
