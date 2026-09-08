# Editor boundary

The editor and the engine communicate through `EditorProtocol.h`. The editor never owns a scene, entity, component, asset, or reflected instance from the engine.

`core/runtime/SceneEditorBridge.*` is the engine-facing adapter. `VulkanRuntime::run()` publishes the initial scene and consumes commands on the engine thread, even while the viewport is not renderable. The bridge publishes another snapshot only after a scene command changes state.

Connected commands: `CreateNode` (child), `RemoveNode` (whole subtree), and `RenameNode`. Every new entity has the engine's default `TransformComponent`; transform/property/component/resource editing is not connected or exposed by the snapshot yet. Entity IDs are encoded as `id + 1` because zero is the editor's invalid node ID. `Scene::getEntities()` contains all entities, including children; parent/child links describe their hierarchy.

Each Scene owns one root Entity. Add Child creates immediately under the selected node; there is no Add Root action. The root cannot be removed or reparented, enforced both by the editor and scene APIs. Creating without an explicit parent attaches to the root; detaching a non-root child reparents it to the root. `Scene::getEntities()` remains a runtime traversal index. Node IDs are allocated when creating/loading entities and remain unchanged by saving. The snapshot identifies the root explicitly with `rootNodeId`. Scene switching is not implemented.

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

The Qt window first shows a startup page. Create Project requires an existing, writable, empty folder (a folder can be created in the native picker); Open Project accepts an existing project folder. Both submit the selected absolute base directory as UTF-8 together with an explicit create/open flag. The create button stays disabled until a location is entered, and invalid or non-empty create locations show an inline error. Cancelling the folder picker keeps the startup page open. Closing the startup page exits without starting Vulkan.

`EngineLoop` waits for that selection and sets `ProjectInfo` before constructing `VulkanRuntime`. After Vulkan services are ready, the runtime constructs AssetManager and SceneManager and calls `createProject()` or `loadProject()` before renderer initialization and the first editor snapshot. A failed project load shows an error dialog, then exits; it never substitutes a blank scene. The editor title shows the folder name, and its status bar shows the save location with a full-path tooltip and selectable text.

New projects immediately save `project.json` (`startupScene`: scene GUID), `assetmap.json` (GUID-to-asset-file mapping), and `assets/Scene/<project-name>_0.json`. Opening restores the asset manifest and deserializes every listed asset into the uninitialized cache without running load hooks. Loading an asset by GUID initializes its cached object on demand. SceneManager resolves the explicit startup scene GUID through AssetManager, rather than choosing the first scene in an unordered cache or resetting the manifest twice.

Scene JSON now stores the Asset base `_name` and a single `_root`. Each Entity stores its name, components, and children. After deserialization, Scene rebuilds the flat entity index, runtime IDs, scene/parent/component ownership links, then runs component load and initialization hooks. Serialization itself never mutates these runtime relationships. **This changes the scene format: old `_entities` scene files and projects without `project.json` are not migrated automatically.**

SDL creates the Vulkan window with `SDL_WINDOW_HIDDEN`. Once runtime initialization is complete, `VulkanRuntime::run()` supplies its native handle to Qt. `QtRuntimeEditorWindow` replaces the startup page with the viewport and editor panels, so the viewport never needs to appear as a separate visible window during startup.

Closing the editor hides the entire window and requests engine exit, leaving the Qt event loop and container alive. After the render loop finishes and the GPU is idle, `EngineLoop` stops Qt. The Qt window is hidden before container destruction, including exits initiated by SDL; Vulkan and SDL cleanup then follows. Rendering still presents directly to the same SDL Vulkan surface on the engine thread.

Manual checks: confirm startup waits for a choice; cancel the folder picker and try an invalid directory; verify Create rejects a non-empty folder; create with an empty folder and check `ProjectInfo::getProjectPath()` before runtime initialization; confirm creation writes `project.json`, `assetmap.json`, and the scene asset; open that folder again; close the startup page and confirm the process exits. After entering the editor, check the save location, confirm there is no standalone SDL window flash; resize and minimize/restore the editor and confirm the viewport follows; exit both with the editor close button and Escape while the viewport has focus, confirming no detached window appears.

## Property conventions

The property widget factory currently understands booleans, arithmetic values, strings, enumerations, and resource references. `EditorProperty::resourceType` selects the resource widget; its value should match `EditorAsset::type`.

Transforms are deliberately represented separately from reflected properties. This keeps Euler-angle UI behavior and transform propagation policy in the future engine adapter instead of baking it into the generic property editor.

## Project round-trip checks (manual)

- Create in an empty folder: exactly one root is shown, selected and expanded; Remove is disabled for it.
- Create siblings and grandchildren, rename them, save twice, then continue editing. Saving must not change runtime IDs or lose the selection.
- Close and open the same folder: hit `SceneManager::loadProject()`, `AssetManager::Init()`, asset JSON deserialization, and `Scene::onLoadAsset()`; verify names, hierarchy, component values and owner links.
- Inspect `_uninitializedAssets` after manifest restoration and `_assets` after requesting the scene: references to the same GUID must resolve to the same object, with initialization once.
- Delete a non-root subtree, save, and reopen: descendants stay deleted and the root remains. Scene API attempts to delete/reparent the root must leave it unchanged.
- Open a folder missing project metadata or a referenced asset, or with invalid JSON: a startup error must appear, with no blank-scene fallback and no overwrite of the existing project.

Compilation and runtime checks are intentionally left to the user per repository instructions.
