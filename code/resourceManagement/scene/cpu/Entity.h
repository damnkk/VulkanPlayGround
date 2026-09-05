#ifndef ENTITY_H
#define ENTITY_H
#include "core/Serializable.h"
#include "resourceManagement/scene/component/Component.h"

namespace Play
{
class Scene;

class Entity : public std::enable_shared_from_this<Entity>
{
public:
    void load();
    void save();
    void init();
    void tick(float deltaTime);

    template <typename TComponent>
    std::shared_ptr<TComponent> TryGetComponent()
    {
        for (auto& component : _components)
        {
            std::shared_ptr<TComponent> cast = std::dynamic_pointer_cast<TComponent>(component);
            if (cast) return cast;
        }
        return nullptr;
    }

    template <typename TComponent>
    std::shared_ptr<TComponent> TryGetComponentInParent(bool self = false)
    {
        if (self)
        {
            std::shared_ptr<TComponent> component = TryGetComponent<TComponent>();
            if (component) return component;
        }

        std::shared_ptr<Entity> entity = _father.lock();
        if (entity) return entity->TryGetComponent<TComponent>(true);
        return nullptr;
    }

    template <typename TComponent>
    bool removeComponent()
    {
        for (int i = 0; i < _components.size(); i++)
        {
            auto&                       component = _components[i];
            std::shared_ptr<TComponent> cast      = std::dynamic_pointer_cast<TComponent>(component);
            if (cast)
            {
                _components.erase(_components.begin() + i);
                return true;
            }
        }
        return false;
    }

    template <typename TComponent, class... Args>
    std::shared_ptr<TComponent> addComponent(Args&&... args)
    {
        std::shared_ptr<TComponent> component = std::make_shared<TComponent>(std::forward<Args>(args)...);
        component->_entity                    = weak_from_this();
        _components.push_back(component);
        return component;
    }

    void                                     addComponent(std::shared_ptr<Component> component);
    std::vector<std::shared_ptr<Component>>& getComponents()
    {
        return _components;
    }

    inline uint32_t getID()
    {
        return _id;
    }
    inline std::string getName()
    {
        return _name;
    }
    void setName(std::string name)
    {
        _name = std::move(name);
    }
    inline std::weak_ptr<Entity> getFather()
    {
        return _father;
    }
    inline std::vector<std::shared_ptr<Entity>> getChildren()
    {
        return _children;
    }
    inline const std::vector<std::shared_ptr<Entity>>& getChildren() const
    {
        return _children;
    }
    inline const std::vector<std::shared_ptr<Component>>& getComponents() const
    {
        return _components;
    }

    void setFather(std::weak_ptr<Entity> father);
    void addChild(std::shared_ptr<Entity> child);
    bool removeChild(std::shared_ptr<Entity> child);

    inline std::shared_ptr<Scene> getScene()
    {
        return _scene.lock();
    }

private:
    uint32_t                                _id   = 0;
    std::string                             _name = "";
    std::vector<std::shared_ptr<Component>> _components;
    std::weak_ptr<Entity>                   _father;
    std::vector<std::shared_ptr<Entity>>    _children;
    std::weak_ptr<Scene>                    _scene;
    friend class Scene;

private:
    // clang-format off
    BeginSerailize()
    SerailizeEntry(_name)
    SerailizeEntry(_components)
    SerailizeEntry(_children)
    for(auto& component:_components) component->_entity = weak_from_this();
    for(auto& child:_children) child->_father = weak_from_this();
    EndSerailize
    //clang-format on
};

} // namespace Play

#endif // ENTITY
