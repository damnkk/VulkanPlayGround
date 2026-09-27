#include "Scene.h"
#include "resourceManagement/scene/component/TransformComponent.h"
#include "nvutils/logger.hpp"
CEREAL_REGISTER_TYPE(Play::Scene)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Asset, Play::Scene)
namespace Play
{

Scene::Scene() : Scene("Scene") {}

Scene::Scene(std::string name) : Asset(name), _root(std::make_shared<Entity>())
{
    _root->_name = getName();
    _root->addComponent<TransformComponent>();
}

Scene::~Scene()
{
    for (auto it = _entities.rbegin(); it != _entities.rend(); ++it) (*it)->exitScene();
    _idPool.destroyAll();
}

void Scene::registerSubtree(const std::shared_ptr<Entity>& entity, const std::shared_ptr<Entity>& parent)
{
    entity->_father = parent;
    _idPool.createID(entity->_id);
    _entities.push_back(entity);
    entity->enterScene(weak_from_this());
    for (const auto& child : entity->_children)
    {
        registerSubtree(child, entity);
    }
}

void Scene::onLoadAsset()
{
    Asset::onLoadAsset();
    if (!_root) throw cereal::Exception("Scene has no root node.");
    for (auto it = _entities.rbegin(); it != _entities.rend(); ++it) (*it)->exitScene();
    _entities.clear();
    _idPool.destroyAll();
    registerSubtree(_root, {});
    for (auto& entity : _entities) entity->load();
    for (auto& entity : _entities) entity->init();
}

bool Scene::onSaveAsset()
{
    if (!Asset::onSaveAsset()) return false;

    for (auto& entity : _entities)
    {
        entity->save();
    }
    return true;
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

std::shared_ptr<Entity> Scene::createEntity(std::string name, std::shared_ptr<Entity> parent)
{
    if (!parent) parent = _root;
    if (!parent || parent->getScene().get() != this) return nullptr;
    auto entity = std::make_shared<Entity>();
    _idPool.createID(entity->_id);
    entity->_name = name.empty() ? "Entity " + std::to_string(entity->_id + 1) : std::move(name);
    entity->addComponent<TransformComponent>();
    _entities.push_back(entity);
    entity->_father = parent;
    parent->_children.push_back(entity);
    entity->enterScene(weak_from_this());
    return entity;
}

bool Scene::addEntity(std::shared_ptr<Entity> entity)
{
    if (!entity || entity->_scene.lock() || entity->_father.lock() || !_root)
    {
        return false;
    }
    _root->_children.push_back(entity);
    registerSubtree(entity, _root);
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
    if (!entity || entity == _root)
    {
        return nullptr;
    }

    // Copy the children because removing each one also detaches it from its parent.
    const auto children = entity->getChildren();
    for (const auto& child : children)
    {
        removeEntity(child->getID());
    }
    entity->exitScene();
    if (auto parent = entity->_father.lock())
    {
        auto& siblings = parent->_children;
        siblings.erase(std::find(siblings.begin(), siblings.end(), entity));
    }
    entity->_father.reset();
    const auto position = std::find(_entities.begin(), _entities.end(), entity);
    _entities.erase(position);
    // Keep IDs unique for queued editor commands; the Scene destructor releases the pool.
    return entity;
}

} // namespace Play
