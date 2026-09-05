#include "Scene.h"
#include "resourceManagement/scene/component/TransformComponent.h"
#include "nvutils/logger.hpp"
CEREAL_REGISTER_TYPE(Play::Scene)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Asset, Play::Scene)
namespace Play
{

void Scene::onLoadAsset()
{
    Asset::onLoadAsset();

    for (auto& entity : _entities)
    {
        entity->load();
        entity->init();
    }
}

void Scene::onSaveAsset()
{
    for (auto& entity : _entities)
    {
        entity->save();
    }
}

void Scene::tick(float deltaTime)
{
    for (auto& entity : _entities)
    {
        for (auto& component : entity->getComponents())
        {
            if (component) component->onUpdate(deltaTime);
        }
    }
}

std::shared_ptr<Entity> Scene::getEntity(uint32_t id)
{
    auto entity = std::find_if(_entities.begin(), _entities.end(), [id](auto& entity) { return entity->_id == id; });
    return entity != _entities.end() ? *entity : nullptr;
}

std::shared_ptr<const Entity> Scene::getEntity(uint32_t id) const
{
    auto entity = std::find_if(_entities.begin(), _entities.end(), [id](const auto& entity) { return entity->_id == id; });
    return entity != _entities.end() ? *entity : nullptr;
}

std::shared_ptr<Entity> Scene::getEntity(std::string name)
{
    auto entity = std::find_if(_entities.begin(), _entities.end(), [name](auto& entity) { return entity->_name == name; });
    return entity != _entities.end() ? *entity : nullptr;
}

std::shared_ptr<Entity> Scene::createEntity(std::string name)
{
    auto entity = std::make_shared<Entity>();
    _idPool.createID(entity->_id);
    entity->_name  = name.empty() ? "Entity " + std::to_string(entity->_id + 1) : std::move(name);
    entity->_scene = weak_from_this();
    entity->addComponent<TransformComponent>();
    _entities.push_back(entity);
    return entity;
}

bool Scene::addEntity(std::shared_ptr<Entity> entity)
{
    if (entity->_scene.lock())
    {
        LOGW("Entity is already belonged to another scene!");
        return false;
    }
    _idPool.createID(entity->_id);
    entity->_scene = weak_from_this();
    _entities.push_back(entity);
    return true;
}

std::shared_ptr<Entity> Scene::removeEntity(std::string name)
{
    auto entity = getEntity(name);
    return entity ? removeEntity(entity->getID()) : nullptr;
}

std::shared_ptr<Entity> Scene::removeEntity(uint32_t id)
{
    auto entity = getEntity(id);
    if (!entity)
    {
        return nullptr;
    }

    // Copy the children because removing each one also detaches it from its parent.
    const auto children = entity->getChildren();
    for (const auto& child : children)
    {
        removeEntity(child->getID());
    }
    entity->setFather({});
    const auto position = std::find(_entities.begin(), _entities.end(), entity);
    _entities.erase(position);
    entity->_scene.reset();
    // Keep IDs unique for queued editor commands; the Scene destructor releases the pool.
    return entity;
}

} // namespace Play
