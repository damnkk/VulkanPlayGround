#ifndef PLAY_CODE_EDITOR_EDITORUIMODEL_H
#define PLAY_CODE_EDITOR_EDITORUIMODEL_H

#include <rttr/type.h>
#include <rttr/instance.h>
#include <rttr/variant.h>

#include <string>
#include <vector>

namespace Play::editor
{

enum class EditorUiPropertyKind
{
    Text,
    Number,
    Bool
};

struct EditorUiProperty
{
    std::string          path;
    std::string          label;
    std::string          typeName;
    std::string          value;
    std::string          defaultValue;
    std::string          step = "1";
    std::string          minimum;
    std::string          maximum;
    EditorUiPropertyKind kind       = EditorUiPropertyKind::Text;
    bool                 editable   = false;
    bool                 hasDefault = false;
    bool                 hasMinimum = false;
    bool                 hasMaximum = false;
    bool                 boolValue  = false;
};

struct EditorUiObject
{
    unsigned int                  id = 0;
    std::string                   title;
    std::string                   typeName;
    bool                          canReset = false;
    std::vector<EditorUiProperty> properties;
};

struct EditorUiKeyValue
{
    std::string label;
    std::string value;
};

struct EditorUiSceneComponent
{
    std::string                   typeName;
    std::vector<EditorUiKeyValue> details;
};

struct EditorUiTransform
{
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3]    = {0.0f, 0.0f, 0.0f};
    float scale[3]       = {1.0f, 1.0f, 1.0f};
};

struct EditorUiSceneNode
{
    std::string                         key;
    std::string                         name;
    std::string                         typeName;
    bool                                is3D                 = false;
    bool                                canAddModelComponent = false;
    EditorUiTransform                   transform;
    std::vector<EditorUiSceneComponent> components;
    std::vector<EditorUiSceneNode>      children;
};

struct EditorUiScene
{
    bool              available = false;
    std::string       emptyText;
    EditorUiSceneNode root;
};

struct EditorUiRenderMode
{
    std::string                 id;
    std::string                 title;
    bool                        active = false;
    EditorUiScene               scene;
    std::vector<EditorUiObject> controls;
};

struct EditorUiSnapshot
{
    std::vector<EditorUiRenderMode> renderModes;
};

namespace detail
{
std::string propertyValueToString(const rttr::variant& value);
void        appendReflectedObjectProperties(rttr::type type, rttr::instance instance, rttr::instance defaultInstance, bool editable,
                                            std::vector<EditorUiProperty>& properties);
bool        setReflectedProperty(rttr::type type, rttr::instance instance, const char* propertyName, const rttr::variant& value);
} // namespace detail

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_EDITORUIMODEL_H
