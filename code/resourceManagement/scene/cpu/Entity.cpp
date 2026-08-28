#include "Entity.h"
#include "nvutils/logger.hpp"

void Entity::Load()
{
    for (auto& component : _components)
    {
        component->onLoad();
    }
}
void Entity::Save()
{
    for (auto& component : _components)
    {
        component->onSave();
    }
}

void Entity::Init()
{
    for (auto& component : _components)
    {
        if (!component->_init) component->onInit();
    }
}

void Entity::Tick(float)
{
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
}

void Entity::setFather(std::weak_ptr<Entity> father)
{
    if (std::shared_ptr<Entity> oldFather = this->_father.lock())
    {
        oldFather->removeChild(shared_from_this());
    }
    this->_father = std::weak_ptr<Entity>(father);

    if (std::shared_ptr<Entity> newFather = father.lock())
    {
        newFather->_children.push_back(shared_from_this());
    }
}

void Entity::addChild(std::shared_ptr<Entity> child)
{
    _children.push_back(child);
    child->setFather(shared_from_this());
}

bool Entity::removeChild(std::shared_ptr<Entity> child)
{
    for (int i = 0; i < _children.size(); i++)
    {
        auto& myChild = _children.at(i);
        if (myChild.get() == child.get())
        {
            myChild->_father = std::weak_ptr<Entity>();
            _children.erase(_children.begin() + i);
            return true;
        }
    }
    return false;
}
