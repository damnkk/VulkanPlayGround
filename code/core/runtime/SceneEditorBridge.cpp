#include "SceneEditorBridge.h"

#include "core/assets/AssetLoadingServer.h"
#include "editor/RuntimeEditor.h"
#include "nvutils/logger.hpp"
#include "resourceManagement/Model.h"
#include "resourceManagement/scene/SceneManager.h"

namespace Play::runtime
{

namespace
{

editor::EditorNodeId toEditorId(uint32_t entityId)
{
    // Entity IDs start at zero; the editor reserves zero for no node.
    return static_cast<editor::EditorNodeId>(entityId) + 1;
}

std::shared_ptr<Entity> findEntity(Scene& scene, editor::EditorNodeId nodeId)
{
    return nodeId == editor::InvalidEditorNodeId ? nullptr : scene.getEntity(static_cast<uint32_t>(nodeId - 1));
}

bool applyCommand(Scene& scene, const editor::EditorCommand& command)
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
        default:
            LOGW("Editor command %d is not connected yet.\n", static_cast<int>(command.type));
            return false;
    }
}

} // namespace

void publishSceneSnapshot(SceneManager& sceneManager, AssetManager& assetManager, editor::RuntimeEditor& editor)
{
    editor::EditorSnapshot snapshot;
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
        [&commands](Scene& scene)
        {
            bool changed = false;
            for (const auto& command : commands)
            {
                changed = applyCommand(scene, command) || changed;
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
