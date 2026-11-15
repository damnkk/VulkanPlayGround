#ifndef PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H
#define PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H

#include "editor/EditorProtocol.h"

namespace Play
{
class AssetManager;
class SceneManager;
} // namespace Play

namespace Play::editor
{
class RuntimeEditor;
}

namespace Play::runtime
{

// Called by the scene owner; Qt only receives copied scene data.
void publishSceneSnapshot(SceneManager& sceneManager, AssetManager& assetManager, editor::RuntimeEditor& editor);
void processSceneCommands(SceneManager& sceneManager, AssetManager& assetManager, editor::RuntimeEditor& editor,
                          const std::vector<editor::EditorCommand>& commands);
void processAssetImports(AssetManager& assetManager, SceneManager& sceneManager, editor::RuntimeEditor& editor,
                         const std::vector<editor::EditorAssetImportRequest>& requests);

// Consumes a UI save request and persists the open project. Kept out of the
// command path because saving locks the scene itself; the runtime loop calls
// this once per request instead of applying it inside editScene().
void saveCurrentProject(SceneManager& sceneManager);

} // namespace Play::runtime

#endif // PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H
