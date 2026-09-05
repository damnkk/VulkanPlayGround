# Editor boundary

The editor and the engine communicate through `EditorProtocol.h`. The editor never owns a scene, entity, component, asset, or reflected instance from the engine.

The engine-facing adapter has two jobs:

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

## Property conventions

The property widget factory currently understands booleans, arithmetic values, strings, enumerations, and resource references. `EditorProperty::resourceType` selects the resource widget; its value should match `EditorAsset::type`.

Transforms are deliberately represented separately from reflected properties. This keeps Euler-angle UI behavior and transform propagation policy in the future engine adapter instead of baking it into the generic property editor.
