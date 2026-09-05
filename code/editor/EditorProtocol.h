#ifndef PLAY_CODE_EDITOR_EDITORPROTOCOL_H
#define PLAY_CODE_EDITOR_EDITORPROTOCOL_H

#include <rttr/type.h>
#include <rttr/variant.h>

namespace Play::editor
{

using EditorNodeId      = uint64_t;
using EditorComponentId = uint64_t;

constexpr EditorNodeId InvalidEditorNodeId = 0;

struct EditorTransform
{
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3]    = {0.0f, 0.0f, 0.0f};
    float scale[3]       = {1.0f, 1.0f, 1.0f};
};

struct EditorProperty
{
    std::string   name;
    std::string   label;
    rttr::type    type = rttr::type::get<void>();
    rttr::variant value;
    bool          readOnly   = false;
    bool          hasMinimum = false;
    bool          hasMaximum = false;
    double        minimum    = 0.0;
    double        maximum    = 0.0;
    double        step       = 0.1;
    std::string   resourceType;
    std::string   resourceId;
};

struct EditorComponent
{
    EditorComponentId           id = 0;
    std::string                 typeName;
    std::string                 label;
    bool                        removable = true;
    std::vector<EditorProperty> properties;
};

struct EditorNode
{
    EditorNodeId                 id           = InvalidEditorNodeId;
    EditorNodeId                 parentId     = InvalidEditorNodeId;
    std::string                  name;
    bool                         hasTransform = false;
    EditorTransform              transform;
    std::vector<EditorComponent> components;
};

struct EditorAsset
{
    std::string id;
    std::string type;
    std::string name;
    std::string path;
};

struct EditorComponentType
{
    std::string typeName;
    std::string label;
};

struct EditorSnapshot
{
    uint64_t                         revision = 0;
    std::string                      sceneName;
    std::vector<EditorNode>          nodes;
    std::vector<EditorAsset>         assets;
    std::vector<EditorComponentType> componentTypes;
};

enum class EditorCommandType
{
    CreateNode,
    RemoveNode,
    RenameNode,
    ReparentNode,
    SetTransform,
    AddComponent,
    RemoveComponent,
    SetProperty,
    BindAsset,
    ClearAsset
};

struct EditorCommand
{
    EditorCommandType type = EditorCommandType::CreateNode;
    EditorNodeId      nodeId      = InvalidEditorNodeId;
    EditorNodeId      parentId    = InvalidEditorNodeId;
    EditorComponentId componentId = 0;
    std::string       name;
    std::string       componentType;
    std::string       property;
    std::string       assetId;
    rttr::variant     value;
    EditorTransform   transform;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_EDITORPROTOCOL_H
