# Editor boundary

The editor and the engine communicate through `EditorProtocol.h`. The editor never owns a scene, entity, component, asset, or reflected instance from the engine.

`core/runtime/SceneEditorBridge.*` is the engine-facing adapter. `VulkanRuntime::run()` publishes the initial scene and consumes commands on the engine thread, even while the viewport is not renderable. The bridge publishes another snapshot after a scene command changes state or an asset import succeeds.

Connected commands: `CreateNode` (child), `RemoveNode` (whole subtree), `RenameNode`, `SetTransform`, `AddComponent`, `RemoveComponent`, `SetProperty`, `BindAsset`, `ClearAsset`, and `InvokeAction`. The Inspector edits local position, Euler rotation in degrees, and scale. Every new entity has a non-removable `TransformComponent`. The component picker uses the bridge's component registry and excludes types already attached to the entity. Static Mesh is currently available in the component picker; Transform is built in and Camera has no component implementation yet. Static Mesh exposes Model selection and Cast Shadow; its renderer submission is still unfinished. Entity IDs are encoded as `id + 1` because zero is the editor's invalid node ID. Component IDs use `ComponentType` under the current one-component-per-type policy. `Scene::getEntities()` contains all entities, including children; parent/child links describe their hierarchy.

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

`reflectProperties()` converts an RTTR-registered object into typed `EditorProperty` values while the engine still owns and locks that object. It understands `ui.label`, `ui.hidden`, `ui.read_only`, `ui.min`, `ui.max`, `ui.step`, and `ui.resource_type`. Resource references are converted to asset IDs and removed from variant values before publishing. `reflectActions()` exposes only no-argument methods marked `ui.action`.

Snapshots contain copied values only. Components are addressed by opaque IDs and reflected property values remain typed as `rttr::variant`. Commands also contain IDs and values rather than engine pointers, so the engine can queue, reject, or translate them without exposing its object lifetime to Qt.

Publish a new snapshot after a command changes the scene. Avoid publishing an unchanged snapshot every frame. The main window owns the UI snapshot and copies a new one only when its revision changes. The Inspector reads that snapshot, updates values in place with signals blocked, and rebuilds only when the selected node or attached components change. RTTR property/action definitions are fixed for each component type during a session. Updates deferred by focused input or an open picker are retried by the existing 200 ms timer, even without a new engine revision.

## Declaring component controls

Use the generic Inspector for ordinary properties and action buttons. Add `RTTR_ENABLE(Component)` to a derived component and register its getter/setter pairs in `reflection/RuntimeReflection.cpp`. Qt widgets stay out of component classes. Only specialized interactions such as curve editors or previews need custom widgets.

```cpp
rttr::registration::class_<Play::StaticMeshComponent>("Play::StaticMeshComponent")
    .property("model", &Play::StaticMeshComponent::getModel, &Play::StaticMeshComponent::setModel)
    (rttr::metadata("ui.label", "Model"), rttr::metadata("ui.resource_type", "Model"))
    .property("castShadow", &Play::StaticMeshComponent::getCastShadow, &Play::StaticMeshComponent::setCastShadow)
    (rttr::metadata("ui.label", "Cast Shadow"));
```

Boolean, number, string, and enum properties use the existing widget factory. Resource properties use a filtered asset picker with a None entry. Register conversion between each resource reference type and `AssetRef` once; ModelRef conversions are already provided. Bind/Clear commands resolve the ID, check the resource type, and call the registered setter on the engine thread. Static Mesh persists its model UUID through `_assetMap`; missing resources load as nullptr.

For a component with a no-argument `rebuild()` operation, add the following to its RTTR registration chain:

```cpp
.method("rebuild", &MyComponent::rebuild)
(rttr::metadata("ui.action", true), rttr::metadata("ui.label", "Rebuild"));
```

This creates a button and dispatches `InvokeAction` through the engine command queue. The action runs under the scene edit lock; it should finish promptly and must not acquire that lock again. No component-specific Inspector branch is required. Properties refresh in place with Qt signals blocked, preserving active text input and open asset pickers. Resource options are rebuilt only when their filtered asset list changes; closing a picker allows a deferred refresh to complete.

Manual verification after building: select Static Mesh, assign a Model, choose None, toggle Cast Shadow, and save/reopen to check persistence. While editing Transform on the same entity, verify that property snapshots do not destroy its editors. Import another Model and confirm it appears in the picker.

## Saving

Saving is a one-shot request, not a scene command: `SceneManager::saveProject()` locks the scene itself, so it must not run inside the `editScene()` lambda that applies commands (that would deadlock). `File > Save` and its Qt shortcut first end the active widget edit, then call `RuntimeEditor::requestSave()` on the Qt thread; Ctrl+S pressed while the embedded viewport has focus is edge-detected by `SdlWindow` (`keySPressed`) and routed the same way. `VulkanRuntime::run()` consumes the request once per frame through `RuntimeEditor::takeSaveRequest()` and calls `SceneEditorBridge::saveCurrentProject()`, which currently delegates to `SceneManager::saveProject()` and logs failures. The save request stays pending if new scene commands or imports arrived after the engine drained its batches; it is consumed only after those batches have been applied.

Manual checks: in the editor, trigger a save from `File > Save`, with Ctrl+S while a Qt panel has focus, and with Ctrl+S while the viewport has focus; each press must save exactly once and the viewport must keep rendering.

## Importing assets

`File > Import Asset...` and the Assets panel Import button share the panel's selected asset type and native multi-file picker. Import requests cross the Qt/engine boundary as copied type/path values and run on the engine thread. The first supported type is Model. A successful import is packaged as a project-local `.vpgmodel` with sibling KTX2 textures, registered in the asset manifest, and published in the next editor snapshot. The Assets panel includes both initialized and deferred assets without forcing deferred assets to allocate runtime or GPU resources.

## Embedded viewport lifecycle

The Qt window first shows a startup page. Create Project requires an existing, writable, empty folder (a folder can be created in the native picker); Open Project accepts an existing project folder. Both submit the selected absolute base directory as UTF-8 together with an explicit create/open flag. The create button stays disabled until a location is entered, and invalid or non-empty create locations show an inline error. Cancelling the folder picker keeps the startup page open. Closing the startup page exits without starting Vulkan.

`RuntimeEditor` carries this selection as path/create values under its existing mutex; there is no receiver interface or Qt-to-host callback. `EngineLoop` waits for that selection and sets `ProjectInfo` before constructing `VulkanRuntime`. After Vulkan services are ready, the runtime constructs AssetManager and SceneManager and calls `createProject()` or `loadProject()` before renderer initialization and the first editor snapshot. A failed project load shows an error dialog, then exits; it never substitutes a blank scene. The editor title shows the folder name, and its status bar shows the save location with a full-path tooltip and selectable text.

New projects immediately save `project.json` (`startupScene`: scene GUID), `assetmap.json` (GUID-to-asset-file mapping), and `assets/Scene/<project-name>_0.json`. Opening restores the asset manifest and deserializes every listed asset into the uninitialized cache without running load hooks. Loading an asset by GUID initializes its cached object on demand. SceneManager resolves the explicit startup scene GUID through AssetManager, rather than choosing the first scene in an unordered cache or resetting the manifest twice.

Scene JSON now stores the Asset base `_name` and a single `_root`. Each Entity stores its name, components, and children. After deserialization, Scene rebuilds the flat entity index, runtime IDs, scene/parent/component ownership links, then runs component load and initialization hooks. Serialization itself never mutates these runtime relationships. **This changes the scene format: old `_entities` scene files and projects without `project.json` are not migrated automatically.**

SDL creates the Vulkan window with `SDL_WINDOW_HIDDEN`. Once runtime initialization is complete, `VulkanRuntime::run()` supplies its native handle to Qt. `QtRuntimeEditorWindow` replaces the startup page with the viewport and editor panels, so the viewport never needs to appear as a separate visible window during startup.

Closing the editor hides the entire window and requests engine exit, leaving the Qt event loop and container alive. After the render loop finishes and the GPU is idle, `EngineLoop` stops Qt. The Qt window is hidden before container destruction, including exits initiated by SDL; Vulkan and SDL cleanup then follows. Rendering still presents directly to the same SDL Vulkan surface on the engine thread.

Manual checks: confirm startup waits for a choice; cancel the folder picker and try an invalid directory; verify Create rejects a non-empty folder; create with an empty folder and check `ProjectInfo::getProjectPath()` before runtime initialization; confirm creation writes `project.json`, `assetmap.json`, and the scene asset; open that folder again; close the startup page and confirm the process exits. After entering the editor, check the save location, confirm there is no standalone SDL window flash; resize and minimize/restore the editor and confirm the viewport follows; exit both with the editor close button and Escape while the viewport has focus, confirming no detached window appears.

## Property conventions

The property widget factory currently understands booleans, arithmetic values, strings, enumerations, and resource references. `EditorProperty::resourceType` selects the resource widget; its value should match `EditorAsset::type`.

Transforms are represented separately from the generic component list. The engine adapter converts quaternions to Euler degrees for snapshots and applies edits through the TransformComponent setters. It excludes the built-in Transform from both the generic Inspector components and the add-component registry. `removable` controls only the Remove button; it never decides which component editor is displayed.

## Project round-trip checks (manual)

- Create in an empty folder: exactly one root is shown, selected and expanded; Remove is disabled for it.
- Create siblings and grandchildren, rename them, save twice, then continue editing. Saving must not change runtime IDs or lose the selection.
- Close and open the same folder: hit `SceneManager::loadProject()`, `AssetManager::Init()`, asset JSON deserialization, and `Scene::onLoadAsset()`; verify names, hierarchy, component values and owner links.
- Inspect `_uninitializedAssets` after manifest restoration and `_assets` after requesting the scene: references to the same GUID must resolve to the same object, with initialization once.
- Delete a non-root subtree, save, and reopen: descendants stay deleted and the root remains. Scene API attempts to delete/reparent the root must leave it unchanged.
- Open a folder missing project metadata or a referenced asset, or with invalid JSON: a startup error must appear, with no blank-scene fallback and no overwrite of the existing project.

Compilation and runtime checks are intentionally left to the user per repository instructions.

## Editor cleanup boundary (2026-09-14)

- The Qt main window owns the current UI snapshot. Inspector keeps a reference to it and only the component IDs needed to recognize structural changes. It does not maintain a second full snapshot or a previous-node copy.
- SceneTree retains items when node IDs and parent links are unchanged, and updates names in place. Its expansion/selection restoration runs only for hierarchy changes. Assets retains its list when resource IDs, names, types and paths are unchanged. Both panels index existing items by ID instead of caching duplicate scene data.
- RuntimeGuiHost starts once per application session. The viewport O key only raises the existing editor; it cannot restart QApplication. The Qt startup page always exists before the engine supplies the viewport handle, so there is no early-handle replay path.
- Startup selection, scene commands, import requests, save and exit use RuntimeEditor's existing communication boundary. Saving remains outside the scene edit lock.

Engine-dependent behavior intentionally retained: the SDL/Vulkan and Qt thread layout, embedded-window shutdown ordering and SDL event pumping; component-type-based IDs and the one-component-per-type rule; whole-transform commands and Euler conversion; engine snapshot publication after editor commands/imports. Scene switching, component-instance identities, continuous engine-driven UI updates and per-command result/acknowledgement are not introduced by this cleanup. Rendering, serialization and component lifecycle implementations are unchanged.

Targeted manual checks after building:

1. Change Position and Cast Shadow while Scene/Assets have selections and scroll positions. The tree/list should retain their items, selection and scroll position.
2. Start a model import, then rename a scene node while it finishes. An asset-only snapshot must not destroy the rename editor.
3. Keep the Model picker open while an import finishes, dismiss it without selecting an item, wait one refresh interval, and reopen it. The new model must be available without another scene edit.
4. Type a Position value and press Ctrl+S without Enter or clicking away. Reopen the project and verify the typed value was saved. Also try a tree rename followed directly by Ctrl+S.
5. Add/remove Static Mesh and switch between entities. Ordinary property updates should retain controls; component/selection changes should rebuild the appropriate Inspector content. Transform remains visible and cannot be added or removed.
6. Exercise Create/Open, close during startup, close the editor after rendering starts, and exit through viewport Escape. O should activate the existing editor. No second QApplication or detached viewport should appear.

This cleanup was reviewed statically; compilation and interactive verification remain manual per repository instructions.
