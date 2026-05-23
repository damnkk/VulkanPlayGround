#ifndef PLAY_EDITOR_EDITOR_REGISTRY_H
#define PLAY_EDITOR_EDITOR_REGISTRY_H

#include <rttr/type.h>
#include <rttr/instance.h>
#include <rttr/variant.h>

#include "editor/EditorHtml.h"

namespace Play
{
template <typename T>
class ControlComponent;
}

namespace Play::editor
{

using EditorObjectId             = unsigned int;
using EditorObjectCapabilityMask = unsigned int;

enum class EditorRenderMode
{
    Any,
    Defer,
    Gaussian,
    Raytrace
};

enum class EditorObjectCapability
{
    None        = 0,
    Editable    = 1 << 0,
    Inspectable = 1 << 1,
    Profiled    = 1 << 2
};

constexpr EditorObjectCapabilityMask toEditorObjectCapabilityMask(EditorObjectCapability capability)
{
    return static_cast<EditorObjectCapabilityMask>(capability);
}

constexpr EditorObjectCapabilityMask getWritableEditorObjectCapabilityMask()
{
    return toEditorObjectCapabilityMask(EditorObjectCapability::Editable) | toEditorObjectCapabilityMask(EditorObjectCapability::Inspectable);
}

constexpr EditorObjectCapabilityMask getReadOnlyEditorObjectCapabilityMask()
{
    return toEditorObjectCapabilityMask(EditorObjectCapability::Inspectable);
}

struct EditorObjectTraits
{
    EditorRenderMode           renderMode     = EditorRenderMode::Any;
    EditorObjectCapabilityMask capabilityMask = getReadOnlyEditorObjectCapabilityMask();
};

struct EditorObjectQuery
{
    EditorRenderMode           renderMode           = EditorRenderMode::Any;
    EditorObjectCapabilityMask requiredCapabilityMask = 0;
};

struct EditorObjectInfo
{
    EditorObjectId     id = 0;
    std::string        title;
    rttr::type         type;
    EditorObjectTraits traits;
};

constexpr EditorObjectTraits makeWritableEditorObjectTraits(EditorRenderMode renderMode = EditorRenderMode::Any)
{
    EditorObjectTraits traits;
    traits.renderMode     = renderMode;
    traits.capabilityMask = getWritableEditorObjectCapabilityMask();
    return traits;
}

constexpr EditorObjectTraits makeReadOnlyEditorObjectTraits(EditorRenderMode renderMode = EditorRenderMode::Any)
{
    EditorObjectTraits traits;
    traits.renderMode     = renderMode;
    traits.capabilityMask = getReadOnlyEditorObjectCapabilityMask();
    return traits;
}

namespace detail
{
class IEditorObjectAdapter
{
public:
    virtual ~IEditorObjectAdapter() = default;

    virtual rttr::type     getType() const                                             = 0;
    virtual rttr::instance getInstance() const                                         = 0;
    virtual rttr::instance getDefaultInstance() const                                  = 0;
    virtual bool           setProperty(const char* propertyName, const rttr::variant& value) = 0;
    virtual bool           resetObject()                                               = 0;
};

template <typename T>
class ObjectEditorAdapter final : public IEditorObjectAdapter
{
public:
    explicit ObjectEditorAdapter(T& object) : _object(&object)
    {
    }

    rttr::type getType() const override
    {
        return rttr::type::get<T>();
    }

    rttr::instance getInstance() const override
    {
        return rttr::instance(*_object);
    }

    rttr::instance getDefaultInstance() const override
    {
        return rttr::instance();
    }

    bool setProperty(const char* propertyName, const rttr::variant& value) override
    {
        return setReflectedProperty(getType(), getInstance(), propertyName, value);
    }

    bool resetObject() override
    {
        return false;
    }

private:
    T* _object = nullptr;
};

template <typename T>
class ControlComponentEditorAdapter final : public IEditorObjectAdapter
{
public:
    explicit ControlComponentEditorAdapter(Play::ControlComponent<T>& component) : _component(&component), _defaultObject(component.getCPUHandle())
    {
    }

    rttr::type getType() const override
    {
        return rttr::type::get<T>();
    }

    rttr::instance getInstance() const override
    {
        return rttr::instance(_component->getCPUHandle());
    }

    rttr::instance getDefaultInstance() const override
    {
        return rttr::instance(_defaultObject);
    }

    bool setProperty(const char* propertyName, const rttr::variant& value) override
    {
        if (!setReflectedProperty(getType(), getInstance(), propertyName, value))
        {
            return false;
        }

        _component->flushToGPU();
        return true;
    }

    bool resetObject() override
    {
        _component->getCPUHandle() = _defaultObject;
        _component->flushToGPU();
        return true;
    }

private:
    Play::ControlComponent<T>* _component = nullptr;
    T                          _defaultObject;
};
} // namespace detail

class EditorRegistry
{
public:
    EditorRegistry();
    ~EditorRegistry();

    EditorRegistry(const EditorRegistry&)            = delete;
    EditorRegistry& operator=(const EditorRegistry&) = delete;
    EditorRegistry(EditorRegistry&&)                 = delete;
    EditorRegistry& operator=(EditorRegistry&&)      = delete;

    template <typename T>
    EditorObjectId registerWritable(const char* title, T& object, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(title, makeWritableEditorObjectTraits(renderMode), new detail::ObjectEditorAdapter<T>(object));
    }

    template <typename T>
    EditorObjectId registerWritable(T& object, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(nullptr, makeWritableEditorObjectTraits(renderMode), new detail::ObjectEditorAdapter<T>(object));
    }

    template <typename T>
    EditorObjectId registerWritable(const char* title, Play::ControlComponent<T>& component, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(title, makeWritableEditorObjectTraits(renderMode), new detail::ControlComponentEditorAdapter<T>(component));
    }

    template <typename T>
    EditorObjectId registerWritable(Play::ControlComponent<T>& component, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(nullptr, makeWritableEditorObjectTraits(renderMode), new detail::ControlComponentEditorAdapter<T>(component));
    }

    template <typename T>
    EditorObjectId registerReadOnly(const char* title, T& object, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(title, makeReadOnlyEditorObjectTraits(renderMode), new detail::ObjectEditorAdapter<T>(object));
    }

    template <typename T>
    EditorObjectId registerReadOnly(T& object, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(nullptr, makeReadOnlyEditorObjectTraits(renderMode), new detail::ObjectEditorAdapter<T>(object));
    }

    template <typename T>
    EditorObjectId registerReadOnly(const char* title, Play::ControlComponent<T>& component, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(title, makeReadOnlyEditorObjectTraits(renderMode), new detail::ControlComponentEditorAdapter<T>(component));
    }

    template <typename T>
    EditorObjectId registerReadOnly(Play::ControlComponent<T>& component, EditorRenderMode renderMode = EditorRenderMode::Any)
    {
        return addObject(nullptr, makeReadOnlyEditorObjectTraits(renderMode), new detail::ControlComponentEditorAdapter<T>(component));
    }

    void clear();

    std::vector<EditorObjectId> queryObjects(const EditorObjectQuery& query) const;
    const EditorObjectInfo*     getObjectInfo(EditorObjectId id) const;
    rttr::instance              getObjectInstance(EditorObjectId id) const;
    rttr::instance              getDefaultObjectInstance(EditorObjectId id) const;
    bool                        setObjectProperty(EditorObjectId id, const char* propertyName, const rttr::variant& value);
    bool                        resetObject(EditorObjectId id);

private:
    EditorObjectId addObject(const char* title, const EditorObjectTraits& traits, detail::IEditorObjectAdapter* adapter);

    struct Impl;
    Impl* _impl = nullptr;
};

} // namespace Play::editor

#endif // PLAY_EDITOR_EDITOR_REGISTRY_H
