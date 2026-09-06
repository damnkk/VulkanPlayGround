#ifndef PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H
#define PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H

namespace Play
{
class SceneManager;
}

namespace Play::editor
{
class RuntimeEditor;
}

namespace Play::runtime
{

// Called on the engine thread; Qt only receives copied scene data.
void publishSceneSnapshot(SceneManager& sceneManager, editor::RuntimeEditor& editor);
void processSceneCommands(SceneManager& sceneManager, editor::RuntimeEditor& editor);

// Consumes a UI save request and persists the open project. Kept out of the
// command path because saving locks the scene itself; the runtime loop calls
// this once per request instead of applying it inside editScene().
void saveCurrentProject(SceneManager& sceneManager);

} // namespace Play::runtime

#endif // PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H
