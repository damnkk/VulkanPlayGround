# Editor boundary

The editor and the engine communicate through `EditorProtocol.h`. The editor never owns a scene, entity, component, asset, or reflected instance from the engine.

`core/runtime/SceneEditorBridge.*` is the engine-facing adapter. `VulkanRuntime::run()` publishes the initial scene and consumes commands on the engine thread, even while the viewport is not renderable. The bridge publishes another snapshot only after a scene command changes state.

Connected commands: `CreateNode` (root or child), `RemoveNode` (whole subtree), and `RenameNode`. Every new entity has the engine's default `TransformComponent`; transform/property/component/resource editing is not connected or exposed by the snapshot yet. Entity IDs are encoded as `id + 1` because zero is the editor's invalid node ID. `Scene::getEntities()` contains all entities, including children; parent/child links describe their hierarchy.

Add Root and Add Child create immediately without a name dialog. `Scene::createEntity()` assigns `Entity 1`, `Entity 2`, etc. when no name is supplied; nodes can be renamed in the tree afterward. IDs are retained until Scene destruction, when the ID pool is cleared.

The adapter has two jobs:

1. Build an `EditorSnapshot` from the current engine state and call `RuntimeEditor::publishSnapshot()` when that state changes.
2. Call `RuntimeEditor::takeCommands()` on the engine thread and apply each command using the engine's normal scene and resource APIs.

```cpp
editor.publishSnapshot(buildEditorSnapshot(scene, assets));

for (EditorCommand& command : editor.takeCommands())
{
    applyEditorCommand(command, scene, assets);
}
```

`reflectProperties()` converts an RTTR-registered object into typed `EditorProperty` values while the engine still owns and locks that object. It understands the small metadata vocabulary `ui.label`, `ui.hidden`, `ui.read_only`, `ui.min`, `ui.max`, `ui.step`, and `ui.resource_type`. The adapter remains responsible for assigning stable node/component IDs and the currently bound resource ID.

Snapshots contain copied values only. Components are addressed by opaque IDs and reflected property values remain typed as `rttr::variant`. Commands also contain IDs and values rather than engine pointers, so the engine can queue, reject, or translate them without exposing its object lifetime to Qt.

Publish a new snapshot after a command changes the scene. Avoid publishing an unchanged snapshot every frame because the Qt inspector intentionally rebuilds only when the snapshot revision changes.

## Saving

Saving is a one-shot request, not a scene command: `SceneManager::saveProject()` locks the scene itself, so it must not run inside the `editScene()` lambda that applies commands (that would deadlock). The toolbar Save button in the single slim action bar calls `RuntimeEditor::requestSave()` on the Qt thread; Ctrl+S pressed while the embedded viewport has focus is edge-detected by `SdlWindow` (`keySPressed`) and routed the same way. `VulkanRuntime::run()` consumes the request once per frame through `RuntimeEditor::takeSaveRequest()` and calls `SceneEditorBridge::saveCurrentProject()`, which currently delegates to `SceneManager::saveProject()` and logs failures.

Manual checks: in the editor, trigger a save from the toolbar button, with Ctrl+S while a Qt panel has focus, and with Ctrl+S while the viewport has focus; each press must save exactly once and the viewport must keep rendering.

## Embedded viewport lifecycle

The Qt window first shows a startup page. Create Project requires an existing, writable, empty folder (a folder can be created in the native picker); Open Project accepts an existing project folder. Both submit the selected absolute base directory as UTF-8. The create button stays disabled until a location is entered, and invalid or non-empty create locations show an inline error. Cancelling the folder picker keeps the startup page open. Closing the startup page exits without starting Vulkan.

`EngineLoop` waits for that selection and sets `ProjectInfo` before constructing `VulkanRuntime`. Both new and existing projects have a non-empty `ProjectInfo::getProjectPath()` during runtime startup. The asset manager accepts a missing manifest as a new project; Save / Ctrl+S writes the scene and asset manifest into the selected directory. The startup UI does not create project files or call scene loading APIs. The editor title shows the folder name, and its status bar shows the save location with a full-path tooltip and selectable text.

SDL creates the Vulkan window with `SDL_WINDOW_HIDDEN`. Once runtime initialization is complete, `VulkanRuntime::run()` supplies its native handle to Qt. `QtRuntimeEditorWindow` replaces the startup page with the viewport and editor panels, so the viewport never needs to appear as a separate visible window during startup.

Closing the editor hides the entire window and requests engine exit, leaving the Qt event loop and container alive. After the render loop finishes and the GPU is idle, `EngineLoop` stops Qt. The Qt window is hidden before container destruction, including exits initiated by SDL; Vulkan and SDL cleanup then follows. Rendering still presents directly to the same SDL Vulkan surface on the engine thread.

Manual checks: confirm startup waits for a choice; cancel the folder picker and try an invalid directory; verify Create rejects a non-empty folder; create with an empty folder and check `ProjectInfo::getProjectPath()` before runtime initialization; save and confirm that `assetmap.json` and the scene asset appear in that folder; open that folder again; close the startup page and confirm the process exits. After entering the editor, check the save location, confirm there is no standalone SDL window flash; resize and minimize/restore the editor and confirm the viewport follows; exit both with the editor close button and Escape while the viewport has focus, confirming no detached window appears.

## Property conventions

The property widget factory currently understands booleans, arithmetic values, strings, enumerations, and resource references. `EditorProperty::resourceType` selects the resource widget; its value should match `EditorAsset::type`.

Transforms are deliberately represented separately from reflected properties. This keeps Euler-angle UI behavior and transform propagation policy in the future engine adapter instead of baking it into the generic property editor.
