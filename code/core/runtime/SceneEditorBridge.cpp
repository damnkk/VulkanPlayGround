#include "SceneEditorBridge.h"

#include "editor/RuntimeEditor.h"
#include "nvutils/logger.hpp"
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

            auto entity = scene.createEntity(command.name);
            entity->setFather(parent);
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

void publishSceneSnapshot(SceneManager& sceneManager, editor::RuntimeEditor& editor)
{
    editor::EditorSnapshot snapshot;
    sceneManager.readScene(
        [&snapshot](const Scene& scene)
        {
            snapshot.sceneName = scene.getName();
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
    editor.publishSnapshot(std::move(snapshot));
}

void processSceneCommands(SceneManager& sceneManager, editor::RuntimeEditor& editor)
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
        publishSceneSnapshot(sceneManager, editor);
    }
}

} // namespace Play::runtime
