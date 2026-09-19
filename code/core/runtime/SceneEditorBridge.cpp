#include "SceneEditorBridge.h"

#include "core/assets/AssetLoadingServer.h"
#include "editor/RuntimeEditor.h"
#include "editor/EditorReflection.h"
#include <rttr/property.h>
#include <rttr/method.h>
#include "nvutils/logger.hpp"
#include "resourceManagement/Model.h"
#include "resourceManagement/scene/SceneManager.h"
#include "resourceManagement/scene/component/TransformComponent.h"
#include "resourceManagement/scene/component/StaticMeshComponent.h"

namespace Play::runtime
{

namespace
{

struct ComponentEntry
{
    ComponentType type;
    const char*   label;
    std::shared_ptr<Component> (*create)();
};

const ComponentEntry componentEntries[] = {
    {STATIC_MESH_COMPONENT, "Static Mesh", []() -> std::shared_ptr<Component> { return std::make_shared<StaticMeshComponent>(); }},
};

editor::EditorNodeId toEditorId(uint32_t entityId)
{
    // Entity IDs start at zero; the editor reserves zero for no node.
    return static_cast<editor::EditorNodeId>(entityId) + 1;
}

std::shared_ptr<Entity> findEntity(Scene& scene, editor::EditorNodeId nodeId)
{
    return nodeId == editor::InvalidEditorNodeId ? nullptr : scene.getEntity(static_cast<uint32_t>(nodeId - 1));
}

bool applyCommand(Scene& scene, AssetManager& assetManager, const editor::EditorCommand& command)
{
    switch (command.type)
    {
        case editor::EditorCommandType::CreateNode:
        {
            auto parent = findEntity(scene, command.parentId);
            if (command.parentId != editor::InvalidEditorNodeId && !parent)
            {
                return false;
            }

            auto entity = scene.createEntity(command.name, parent);
            if (!entity) return false;
            entity->init();
            return true;
        }
        case editor::EditorCommandType::RemoveNode:
        {
            auto entity = findEntity(scene, command.nodeId);
            return entity && scene.removeEntity(entity->getID()) != nullptr;
        }
        case editor::EditorCommandType::RenameNode:
        {
            auto entity = findEntity(scene, command.nodeId);
            if (!entity)
            {
                return false;
            }
            entity->setName(command.name);
            return true;
        }
        case editor::EditorCommandType::SetTransform:
        {
            auto entity    = findEntity(scene, command.nodeId);
            auto transform = entity ? entity->TryGetComponent<TransformComponent>() : nullptr;
            if (!transform) return false;
            const auto& value = command.transform;
            transform->setPosition({value.translation[0], value.translation[1], value.translation[2]});
            transform->setRotate(glm::vec3(value.rotation[0], value.rotation[1], value.rotation[2]));
            transform->setScale({value.scale[0], value.scale[1], value.scale[2]});
            return true;
        }
        case editor::EditorCommandType::AddComponent:
        {
            auto entity = findEntity(scene, command.nodeId);
            if (!entity) return false;
            for (const auto& entry : componentEntries)
            {
                if (command.componentType != std::to_string(entry.type)) continue;
                for (const auto& component : entity->getComponents())
                {
                    if (component->getType() == entry.type) return false;
                }
                auto component = entry.create();
                entity->addComponent(component);
                component->onLoad();
                entity->init();
                return true;
            }
            return false;
        }
        case editor::EditorCommandType::RemoveComponent:
        {
            auto entity = findEntity(scene, command.nodeId);
            if (!entity || command.componentId == TRANSFORM_COMPONENT) return false;
            auto& components = entity->getComponents();
            for (auto iter = components.begin(); iter != components.end(); ++iter)
            {
                if (static_cast<editor::EditorComponentId>((*iter)->getType()) == command.componentId)
                {
                    components.erase(iter);
                    return true;
                }
            }
            return false;
        }
        case editor::EditorCommandType::SetProperty:
        case editor::EditorCommandType::BindAsset:
        case editor::EditorCommandType::ClearAsset:
        case editor::EditorCommandType::InvokeAction:
        {
            auto entity = findEntity(scene, command.nodeId);
            if (!entity) return false;
            for (const auto& component : entity->getComponents())
            {
                if (static_cast<editor::EditorComponentId>(component->getType()) != command.componentId) continue;
                rttr::instance instance(*component);
                const auto     type = instance.get_derived_type();
                if (command.type == editor::EditorCommandType::InvokeAction)
                {
                    const auto method = type.get_method(command.property);
                    return method.is_valid() && method.get_metadata("ui.action").to_bool() && method.get_parameter_infos().empty() &&
                           method.invoke(instance).is_valid();
                }
                const auto property = type.get_property(command.property);
                if (!property.is_valid() || property.is_readonly() || property.get_metadata("ui.read_only").to_bool() ||
                    property.get_metadata("ui.hidden").to_bool())
                    return false;
                const auto    resourceType = property.get_metadata("ui.resource_type");
                rttr::variant value;
                if (command.type == editor::EditorCommandType::SetProperty)
                {
                    if (resourceType.is_valid()) return false;
                    value = command.value;
                }
                else
                {
                    if (!resourceType.is_valid()) return false;
                    AssetRef asset;
                    if (command.type == editor::EditorCommandType::BindAsset)
                    {
                        const auto uid = uuids::uuid::from_string(command.assetId);
                        if (!uid) return false;
                        asset = assetManager.getAsset(*uid);
                        if (!asset || asset->getAssetTypeName() != resourceType.to_string()) return false;
                    }
                    value = asset;
                }
                return value.convert(property.get_type()) && property.set_value(instance, value);
            }
            return false;
        }
        default:
            LOGW("Editor command %d is not connected yet.\n", static_cast<int>(command.type));
            return false;
    }
}

} // namespace

void publishSceneSnapshot(SceneManager& sceneManager, AssetManager& assetManager, editor::RuntimeEditor& editor)
{
    editor::EditorSnapshot snapshot;
    for (const auto& entry : componentEntries)
    {
        snapshot.componentTypes.push_back({std::to_string(entry.type), entry.label});
    }
    sceneManager.readScene(
        [&snapshot](const Scene& scene)
        {
            snapshot.sceneName  = scene.getName();
            snapshot.rootNodeId = toEditorId(scene.getRoot()->getID());
            for (const auto& entity : scene.getEntities())
            {
                editor::EditorNode node;
                node.id   = toEditorId(entity->getID());
                node.name = entity->getName();
                if (auto parent = entity->getFather().lock())
                {
                    node.parentId = toEditorId(parent->getID());
                }
                if (auto transform = entity->TryGetComponent<TransformComponent>())
                {
                    node.hasTransform   = true;
                    const auto position = transform->getPosition();
                    const auto rotation = glm::degrees(glm::eulerAngles(transform->getRotate()));
                    const auto scale    = transform->getScale();
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        node.transform.translation[axis] = position[axis];
                        node.transform.rotation[axis]    = rotation[axis];
                        node.transform.scale[axis]       = scale[axis];
                    }
                }
                for (const auto& component : entity->getComponents())
                {
                    // Transform is already represented by node.transform and its dedicated editor.
                    if (component->getType() == TRANSFORM_COMPONENT) continue;
                    editor::EditorComponent item;
                    item.id       = component->getType();
                    item.typeName = std::to_string(component->getType());
                    item.label    = component->getTypeName();
                    if (item.label.empty()) item.label = item.typeName;
                    rttr::instance instance(*component);
                    editor::reflectProperties(instance.get_derived_type(), instance, item.properties);
                    editor::reflectActions(instance.get_derived_type(), item.actions);
                    node.components.push_back(std::move(item));
                }
                snapshot.nodes.push_back(std::move(node));
            }
        });
    for (const auto& asset : assetManager.getRegisteredAssets())
    {
        if (!asset) continue;
        editor::EditorAsset editorAsset;
        editorAsset.id   = uuids::to_string(asset->getUID());
        editorAsset.type = asset->getAssetTypeName();
        editorAsset.name = asset->getName();
        editorAsset.path = asset->getFilePath();
        snapshot.assets.push_back(std::move(editorAsset));
    }
    editor.publishSnapshot(std::move(snapshot));
}

void saveCurrentProject(SceneManager& sceneManager)
{
    // Extension point for the real save flow: serialize the scene into the
    // active project directory and flush its asset manifest. The existing
    // SceneManager::saveProject() covers that path for now.
    std::string errorMessage;
    if (!sceneManager.saveProject(&errorMessage))
    {
        LOGW("Save project failed: %s\n", errorMessage.empty() ? "unknown error" : errorMessage.c_str());
    }
}

void processSceneCommands(SceneManager& sceneManager, AssetManager& assetManager, editor::RuntimeEditor& editor)
{
    const auto commands = editor.takeCommands();
    if (commands.empty())
    {
        return;
    }

    const bool changed = sceneManager.editScene(
        [&commands, &assetManager](Scene& scene)
        {
            bool changed = false;
            for (const auto& command : commands)
            {
                changed = applyCommand(scene, assetManager, command) || changed;
            }
            return changed;
        });
    if (changed)
    {
        publishSceneSnapshot(sceneManager, assetManager, editor);
    }
}

void processAssetImports(AssetManager& assetManager, SceneManager& sceneManager, editor::RuntimeEditor& editor)
{
    const auto requests = editor.takeAssetImportRequests();
    if (requests.empty()) return;

    bool changed = false;
    for (const auto& request : requests)
    {
        try
        {
            switch (request.type)
            {
                case editor::EditorAssetType::Model:
                    if (assetManager.importAsset<Model>(request.sourcePath))
                    {
                        LOGI("Imported model asset {%s}\n", request.sourcePath.c_str());
                        changed = true;
                    }
                    else
                    {
                        LOGW("Failed to import model asset {%s}\n", request.sourcePath.c_str());
                    }
                    break;
            }
        }
        catch (const std::exception& error)
        {
            LOGW("Failed to import asset {%s}: %s\n", request.sourcePath.c_str(), error.what());
        }
    }

    if (changed)
    {
        publishSceneSnapshot(sceneManager, assetManager, editor);
    }
}

} // namespace Play::runtime
