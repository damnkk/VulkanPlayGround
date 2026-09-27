#include "Entity.h"
#include "Scene.h"
#include "nvutils/logger.hpp"

namespace Play
{

void Entity::enterScene(std::weak_ptr<Scene> scene)
{
    _scene = scene;
    for (auto& component : _components) component->_entity = weak_from_this();
    for (auto& component : _components) component->onEnterScene();
}

void Entity::exitScene()
{
    for (auto& component : _components) component->onExitScene();
    _scene.reset();
}

void Entity::load()
{
    for (auto& component : _components)
    {
        component->onLoad();
    }
}
void Entity::save()
{
    for (auto& component : _components)
    {
        component->onSave();
    }
}

void Entity::init()
{
    for (auto& component : _components)
    {
        if (!component->_init)
        {
            component->onInit();
            component->_init = true;
        }
    }
}

void Entity::tick(float deltaTime)
{
    for (auto& component : _components)
    {
        if (component)
        {
            component->onUpdate(deltaTime);
        }
    }
}

void Entity::addComponent(std::shared_ptr<Component> component)
{
    if (std::shared_ptr<Entity> owner = component->_entity.lock())
    {
        LOGD("Entity %s already has component %s", owner->getName().c_str(), component->getTypeName().c_str());
        return;
    }
    _components.push_back(component);
    component->_entity = weak_from_this();
    if (!_scene.expired()) component->onEnterScene();
}

void Entity::setFather(std::weak_ptr<Entity> father)
{
    auto parent = father.lock();
    if (auto scene = _scene.lock())
    {
        if (scene->getRoot().get() == this) return;
        if (!parent) parent = scene->getRoot();
        if (parent->getScene() != scene) return;
    }
    else if (parent && parent->getScene())
    {
        // Scene::addEntity registers an unowned subtree before attaching it.
        return;
    }
    for (auto ancestor = parent; ancestor; ancestor = ancestor->_father.lock())
    {
        if (ancestor.get() == this) return;
    }
    auto oldParent = _father.lock();
    if (oldParent == parent) return;
    if (oldParent)
    {
        auto& siblings = oldParent->_children;
        siblings.erase(std::find(siblings.begin(), siblings.end(), shared_from_this()));
    }
    _father = parent;
    if (parent) parent->_children.push_back(shared_from_this());
}

void Entity::addChild(std::shared_ptr<Entity> child)
{
    child->setFather(shared_from_this());
}

bool Entity::removeChild(std::shared_ptr<Entity> child)
{
    if (!child || child->_father.lock().get() != this) return false;
    if (auto scene = _scene.lock(); scene && scene->getRoot().get() == this) return false;
    child->setFather({});
    return true;
}

} // namespace Play
