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

} // namespace Play::runtime

#endif // PLAY_CODE_CORE_RUNTIME_SCENEEDITORBRIDGE_H
