#ifndef COMPONENT_H
#define COMPONENT_H

#include "rttr/rttr_enable.h"
#include <memory>
#include "core/Serializable.h"
namespace Play
{
class Entity;

enum ComponentType
{
    UNDEFINED_COMPONENT = 0,
    TRANSFORM_COMPONENT,
    CAMERA_COMPONENT,
    STATIC_MESH_COMPONENT,

    COMPONENT_TYPE_MAX_ENUM
};
class Component
{
public:
    Component() = default;
    virtual ~Component() {};

    virtual void onLoad() {};
    virtual void onSave() {};
    virtual void onInit() {};
    // Called after the entity joins a scene, or when attached to an entity already in a scene.
    virtual void onEnterScene() {};
    // Called before detaching. During Scene destruction, getScene() may already be expired.
    virtual void onExitScene() {};
    virtual void onUpdate(float deltaTime) = 0;
    inline bool  inited()
    {
        return _init;
    }
    virtual std::string getTypeName()
    {
        return "Undefined";
    }
    virtual ComponentType getType()
    {
        return UNDEFINED_COMPONENT;
    }

    inline std::shared_ptr<Entity> getEntity()
    {
        return _entity.lock();
    }

    template <typename TComponent>
    std::shared_ptr<TComponent> tryGetComponent()
    {
        return nullptr;
    }

    template <typename TComponent>
    std::shared_ptr<TComponent> tryGetComponentInParent(bool = false)
    {
        return nullptr;
    }
    RTTR_ENABLE();

private:
    bool                  _init = false;
    std::weak_ptr<Entity> _entity;

    friend class Entity;

private:
    // clang-format off
    BeginSerailize()
    EndSerailize
    // clang-format on
};
} // namespace Play

#endif // COMPONENT_H
