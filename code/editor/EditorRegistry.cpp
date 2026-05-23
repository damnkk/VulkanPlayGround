#include "editor/EditorRegistry.h"

namespace Play::editor
{

namespace
{
bool matchesRenderMode(EditorRenderMode requestedMode, EditorRenderMode objectMode)
{
    return requestedMode == EditorRenderMode::Any || objectMode == EditorRenderMode::Any || requestedMode == objectMode;
}

bool matchesCapabilities(EditorObjectCapabilityMask requiredCapabilityMask, EditorObjectCapabilityMask objectCapabilityMask)
{
    return (objectCapabilityMask & requiredCapabilityMask) == requiredCapabilityMask;
}

std::string makeObjectTitle(const char* title, rttr::type type)
{
    if (title && title[0])
    {
        return title;
    }

    const rttr::variant label = type.get_metadata("ui.label");
    if (label.is_valid())
    {
        bool              ok        = false;
        const std::string labelText = label.to_string(&ok);
        if (ok && !labelText.empty())
        {
            return labelText;
        }
    }

    if (type.is_valid())
    {
        return type.get_name().to_string();
    }

    return "Editor Object";
}
} // namespace

struct EditorRegistry::Impl
{
    struct Entry
    {
        Entry(
            EditorObjectId id,
            const char* title,
            const EditorObjectTraits& traits,
            rttr::type type,
            std::unique_ptr<detail::IEditorObjectAdapter> inputAdapter)
            : info{id, makeObjectTitle(title, type), type, traits}, adapter(std::move(inputAdapter))
        {
        }

        Entry(Entry&&) noexcept            = default;
        Entry& operator=(Entry&&) noexcept = default;
        Entry(const Entry&)                = delete;
        Entry& operator=(const Entry&)     = delete;

        EditorObjectInfo                       info;
        std::unique_ptr<detail::IEditorObjectAdapter> adapter;
    };

    std::vector<Entry> objects;

    Entry* entryFromId(EditorObjectId id)
    {
        if (id == 0)
        {
            return nullptr;
        }

        const size_t index = static_cast<size_t>(id - 1);
        if (index >= objects.size())
        {
            return nullptr;
        }

        return &objects[index];
    }

    const Entry* entryFromId(EditorObjectId id) const
    {
        if (id == 0)
        {
            return nullptr;
        }

        const size_t index = static_cast<size_t>(id - 1);
        if (index >= objects.size())
        {
            return nullptr;
        }

        return &objects[index];
    }
};

EditorRegistry::EditorRegistry() : _impl(new Impl())
{
}

EditorRegistry::~EditorRegistry()
{
    delete _impl;
    _impl = nullptr;
}

void EditorRegistry::clear()
{
    _impl->objects.clear();
}

std::vector<EditorObjectId> EditorRegistry::queryObjects(const EditorObjectQuery& query) const
{
    std::vector<EditorObjectId> objects;
    for (const Impl::Entry& entry : _impl->objects)
    {
        if (matchesRenderMode(query.renderMode, entry.info.traits.renderMode)
            && matchesCapabilities(query.requiredCapabilityMask, entry.info.traits.capabilityMask))
        {
            objects.push_back(entry.info.id);
        }
    }

    return objects;
}

const EditorObjectInfo* EditorRegistry::getObjectInfo(EditorObjectId id) const
{
    const Impl::Entry* entry = _impl->entryFromId(id);
    return entry ? &entry->info : nullptr;
}

rttr::instance EditorRegistry::getObjectInstance(EditorObjectId id) const
{
    const Impl::Entry* entry = _impl->entryFromId(id);
    return entry ? entry->adapter->getInstance() : rttr::instance();
}

rttr::instance EditorRegistry::getDefaultObjectInstance(EditorObjectId id) const
{
    const Impl::Entry* entry = _impl->entryFromId(id);
    return entry ? entry->adapter->getDefaultInstance() : rttr::instance();
}

bool EditorRegistry::setObjectProperty(EditorObjectId id, const char* propertyName, const rttr::variant& value)
{
    Impl::Entry* entry = _impl->entryFromId(id);
    if (!entry)
    {
        return false;
    }

    if (!matchesCapabilities(toEditorObjectCapabilityMask(EditorObjectCapability::Editable), entry->info.traits.capabilityMask))
    {
        return false;
    }

    return entry->adapter->setProperty(propertyName, value);
}

bool EditorRegistry::resetObject(EditorObjectId id)
{
    Impl::Entry* entry = _impl->entryFromId(id);
    if (!entry)
    {
        return false;
    }

    if (!matchesCapabilities(toEditorObjectCapabilityMask(EditorObjectCapability::Editable), entry->info.traits.capabilityMask))
    {
        return false;
    }

    return entry->adapter->resetObject();
}

EditorObjectId EditorRegistry::addObject(const char* title, const EditorObjectTraits& traits, detail::IEditorObjectAdapter* adapter)
{
    const EditorObjectId id         = static_cast<EditorObjectId>(_impl->objects.size() + 1);
    auto                 adapterPtr = std::unique_ptr<detail::IEditorObjectAdapter>(adapter);
    const rttr::type     type       = adapterPtr->getType();
    _impl->objects.emplace_back(id, title, traits, type, std::move(adapterPtr));
    return id;
}

} // namespace Play::editor
