#include "editor/SceneManagerEditor.h"

#include "editor/EditorHtml.h"
#include "resourceManagement/SceneManager.h"

namespace Play::editor
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;

bool isSameText(const char* lhs, const char* rhs)
{
    return std::string(lhs ? lhs : "") == std::string(rhs ? rhs : "");
}

float radiansToDegrees(float radians)
{
    return radians * 180.0f / kPi;
}

float degreesToRadians(float degrees)
{
    return degrees * kPi / 180.0f;
}

std::string makeNodeKey(CpuSceneNodeID nodeID)
{
    return std::to_string(nodeID.index) + ":" + std::to_string(nodeID.generation);
}

bool parseUInt(const std::string& text, size_t& cursor, uint32_t& value)
{
    if (cursor >= text.size() || text[cursor] < '0' || text[cursor] > '9')
    {
        return false;
    }

    uint32_t parsed = 0;
    while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9')
    {
        parsed = parsed * 10 + static_cast<uint32_t>(text[cursor] - '0');
        ++cursor;
    }

    value = parsed;
    return true;
}

bool parseNodeKey(const char* key, CpuSceneNodeID& nodeID)
{
    const std::string text = key ? key : "";
    size_t            cursor = 0;
    uint32_t          index = INVALID_SCENE_ID;
    uint32_t          generation = 0;
    if (!parseUInt(text, cursor, index) || cursor >= text.size() || text[cursor] != ':')
    {
        return false;
    }

    ++cursor;
    if (!parseUInt(text, cursor, generation) || cursor != text.size())
    {
        return false;
    }

    nodeID.index      = index;
    nodeID.generation = generation;
    return true;
}

bool parseFloat(const char* text, float& value)
{
    bool ok = false;
    value   = rttr::variant(std::string(text ? text : "")).to_float(&ok);
    return ok;
}

bool setVectorComponent(glm::vec3& vector, const std::string& component, float value)
{
    if (component == "x")
    {
        vector.x = value;
        return true;
    }

    if (component == "y")
    {
        vector.y = value;
        return true;
    }

    if (component == "z")
    {
        vector.z = value;
        return true;
    }

    return false;
}

const char* nodeTypeLabel(CpuSceneNodeType type)
{
    return type == CpuSceneNodeType::eNode3D ? "3D Node" : "2D Node";
}

const char* loadStateLabel(CpuModelComponent::LoadState state)
{
    switch (state)
    {
        case CpuModelComponent::LoadState::eQueued:
            return "Queued";
        case CpuModelComponent::LoadState::eLoading:
            return "Loading";
        case CpuModelComponent::LoadState::eLoaded:
            return "Loaded";
        case CpuModelComponent::LoadState::eFailed:
            return "Failed";
        case CpuModelComponent::LoadState::eEmpty:
        default:
            return "Empty";
    }
}

void appendNodeKeyAttribute(std::string& html, const char* attributeName, CpuSceneNodeID nodeID)
{
    html += " ";
    html += attributeName;
    html += "=\"";
    detail::appendHtmlText(html, makeNodeKey(nodeID));
    html += "\"";
}

void appendSceneTransformInput(std::string& html, CpuSceneNodeID nodeID, const char* propertyPath, float value, const char* step)
{
    html += "<input type=\"text\" value=\"";
    detail::appendHtmlText(html, std::to_string(value));
    html += "\" inputmode=\"decimal\" data-editor-drag=\"number\" data-editor-step=\"";
    detail::appendHtmlText(html, step);
    html += "\" data-scene-node-transform=\"";
    detail::appendHtmlText(html, propertyPath);
    html += "\" data-scene-node=\"";
    detail::appendHtmlText(html, makeNodeKey(nodeID));
    html += "\" data-editor-default=\"";
    detail::appendHtmlText(html, std::to_string(value));
    html += "\" title=\"";
    detail::appendHtmlText(html, propertyPath);
    html += "\">";
}

void appendTransformRow(std::string& html, CpuSceneNodeID nodeID, const char* label, const char* propertyName, glm::vec3 value, const char* step)
{
    static const char* componentNames[] = {"x", "y", "z"};

    html += "<div class=\"property-row scene-transform-row\"><label>";
    detail::appendHtmlText(html, label);
    html += "</label><div class=\"vector-input vector-size-3\">";
    for (const char* componentName : componentNames)
    {
        const std::string propertyPath = std::string(propertyName) + "." + componentName;
        float             componentValue = value.x;
        if (componentName[0] == 'y')
        {
            componentValue = value.y;
        }
        else if (componentName[0] == 'z')
        {
            componentValue = value.z;
        }
        appendSceneTransformInput(html, nodeID, propertyPath.c_str(), componentValue, step);
    }
    html += "</div></div>";
}

void appendTextInfoRow(std::string& html, const char* label, const std::string& value)
{
    html += "<div class=\"component-info-row\"><span>";
    detail::appendHtmlText(html, label);
    html += "</span><span>";
    detail::appendHtmlText(html, value);
    html += "</span></div>";
}

void appendTextInfoRow(std::string& html, const char* label, const char* value)
{
    appendTextInfoRow(html, label, std::string(value ? value : ""));
}

void appendComponentList(std::string& html, const CpuScene& scene, CpuSceneNodeID nodeID)
{
    const CpuModelComponent* modelComponent = scene.getComponent<CpuModelComponent>(nodeID);
    html += "<div class=\"component-list\">";
    if (!modelComponent)
    {
        html += "<div class=\"empty\">No components.</div>";
        html += "</div>";
        return;
    }

    html += "<article class=\"component-card\" data-scene-component-card=\"ModelComponent\">";
    html += "<div class=\"component-title\">ModelComponent</div>";
    appendTextInfoRow(html, "Path", modelComponent->sourcePath.empty() ? "-" : modelComponent->sourcePath);
    appendTextInfoRow(html, "State", loadStateLabel(modelComponent->loadState));
    appendTextInfoRow(html, "Renderables", modelComponent->usesAllRenderables() ? "All" : std::to_string(modelComponent->renderableCount));
    if (!modelComponent->loadMessage.empty())
    {
        appendTextInfoRow(html, "Message", modelComponent->loadMessage);
    }
    html += "</article></div>";
}

void appendSceneNodeInspector(std::string& html, const CpuScene& scene, CpuSceneNodeID nodeID, CpuSceneNodeID selectedNode)
{
    const CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return;
    }

    const std::string key = makeNodeKey(nodeID);
    html += "<article class=\"scene-node-inspector";
    html += key == makeNodeKey(selectedNode) ? " active" : "";
    html += "\"";
    appendNodeKeyAttribute(html, "data-scene-node-panel", nodeID);
    html += "><div class=\"object-head scene-node-head\"><div><div class=\"object-title\">";
    detail::appendHtmlText(html, node->name);
    html += "</div><div class=\"object-type\">";
    detail::appendHtmlText(html, nodeTypeLabel(node->type));
    html += "</div></div></div>";

    html += "<section class=\"inspector-section\"><div class=\"section-title\">Transform</div><div class=\"property-list\">";
    appendTransformRow(html, nodeID, "Translation", "translation", node->local.translation, "0.01");
    appendTransformRow(html, nodeID, "Rotation", "rotation", glm::vec3(radiansToDegrees(node->local.rotation.x), radiansToDegrees(node->local.rotation.y),
                                                                        radiansToDegrees(node->local.rotation.z)),
                       "0.1");
    appendTransformRow(html, nodeID, "Scale", "scale", node->local.scale, "0.01");
    html += "</div></section>";

    const bool hasModelComponent = scene.getComponent<CpuModelComponent>(nodeID) != nullptr;
    const bool canAddModel       = node->type == CpuSceneNodeType::eNode3D && !hasModelComponent;
    html += "<section class=\"inspector-section\"><div class=\"section-head\"><div class=\"section-title\">Components</div>";
    html += "<button class=\"scene-add-component\" type=\"button\" data-scene-add-component=\"ModelComponent\" title=\"Add ModelComponent\"";
    if (!canAddModel)
    {
        html += " disabled";
    }
    html += ">+</button></div>";
    appendComponentList(html, scene, nodeID);
    html += "</section></article>";
}

void appendSceneNodeInspectorsRecursive(std::string& html, const CpuScene& scene, CpuSceneNodeID nodeID, CpuSceneNodeID selectedNode)
{
    const CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return;
    }

    appendSceneNodeInspector(html, scene, nodeID, selectedNode);

    CpuSceneNodeID childID = node->firstChild;
    while (scene.isValid(childID))
    {
        const CpuSceneNode* child = scene.getNode(childID);
        const CpuSceneNodeID nextID = child ? child->nextSibling : CpuSceneNodeID{};
        appendSceneNodeInspectorsRecursive(html, scene, childID, selectedNode);
        childID = nextID;
    }
}

void appendSceneNodeTreeRecursive(std::string& html, const CpuScene& scene, CpuSceneNodeID nodeID, CpuSceneNodeID selectedNode, bool root)
{
    const CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return;
    }

    const bool        hasChildren = scene.isValid(node->firstChild);
    const std::string key         = makeNodeKey(nodeID);
    html += "<div class=\"scene-tree-item";
    html += root ? " expanded" : "";
    html += "\"";
    appendNodeKeyAttribute(html, "data-scene-node-item", nodeID);
    html += "><div class=\"scene-tree-line\"><button class=\"scene-tree-toggle\" type=\"button\" data-scene-tree-toggle";
    if (!hasChildren)
    {
        html += " disabled";
    }
    html += ">";
    detail::appendHtmlText(html, hasChildren ? (root ? "v" : ">") : "");
    html += "</button><button class=\"scene-tree-node";
    html += key == makeNodeKey(selectedNode) ? " selected" : "";
    html += "\" type=\"button\"";
    appendNodeKeyAttribute(html, "data-scene-node-select", nodeID);
    html += "><span class=\"scene-node-type\">";
    detail::appendHtmlText(html, node->type == CpuSceneNodeType::eNode3D ? "3D" : "2D");
    html += "</span><span class=\"scene-node-name\">";
    detail::appendHtmlText(html, node->name);
    html += "</span></button></div><div class=\"scene-tree-children\">";

    CpuSceneNodeID childID = node->firstChild;
    while (scene.isValid(childID))
    {
        const CpuSceneNode* child = scene.getNode(childID);
        const CpuSceneNodeID nextID = child ? child->nextSibling : CpuSceneNodeID{};
        appendSceneNodeTreeRecursive(html, scene, childID, selectedNode, false);
        childID = nextID;
    }

    html += "</div></div>";
}
} // namespace

SceneManagerEditor::SceneManagerEditor(const char* renderModeId) : _renderModeId(renderModeId ? renderModeId : "")
{
}

void SceneManagerEditor::setSceneManager(Play::SceneManager* sceneManager)
{
    _sceneManager = sceneManager;
}

void SceneManagerEditor::appendHtml(std::string& html) const
{
    html += "<section class=\"scene-manager-editor\" data-scene-manager-editor data-render-mode-id=\"";
    detail::appendHtmlText(html, _renderModeId);
    html += "\"><section class=\"panel scene-tree\"><div class=\"panel-head\"><h2>Scene Tree</h2></div>";
    if (!_sceneManager)
    {
        html += "<div class=\"empty\">No SceneManager for this render mode.</div>";
        html += "</section><section class=\"panel inspector\"><h2>Inspector</h2><div class=\"empty\">No scene selection.</div></section></section>";
        return;
    }

    _sceneManager->readSceneGraph(
        [&](const CpuScene& scene)
        {
            const CpuSceneNodeID selectedNode = scene.rootNode();
            html += "<div class=\"scene-tree-list\">";
            appendSceneNodeTreeRecursive(html, scene, scene.rootNode(), selectedNode, true);
            html += "</div></section><section class=\"panel inspector\"><h2>Inspector</h2><div class=\"scene-node-inspector-list\">";
            appendSceneNodeInspectorsRecursive(html, scene, scene.rootNode(), selectedNode);
            html += "</div></section>";
        });
    html += "</section>";
}

std::string SceneManagerEditor::createSceneNode(const char* parentNodeKey, const char* nodeType)
{
    if (!_sceneManager)
    {
        return {};
    }

    CpuSceneNodeID parentNodeID;
    const bool     hasParentNode = parseNodeKey(parentNodeKey, parentNodeID);

    return _sceneManager->editSceneGraph(
        [&](CpuScene& scene)
        {
            if (!hasParentNode || !scene.isValid(parentNodeID))
            {
                parentNodeID = scene.rootNode();
            }

            CpuSceneNodeID nodeID;
            if (isSameText(nodeType, "Node2D"))
            {
                nodeID = scene.create2DNode("Node2D", parentNodeID);
            }
            else if (isSameText(nodeType, "Node3D"))
            {
                nodeID = scene.create3DNode("Node3D", parentNodeID);
            }
            else
            {
                return std::string();
            }

            return scene.isValid(nodeID) ? makeNodeKey(nodeID) : std::string();
        });
}

bool SceneManagerEditor::setSceneNodeTransform(const char* nodeKey, const char* transformPath, const char* value)
{
    if (!_sceneManager)
    {
        return false;
    }

    CpuSceneNodeID nodeID;
    float          componentValue = 0.0f;
    if (!parseNodeKey(nodeKey, nodeID) || !parseFloat(value, componentValue))
    {
        return false;
    }

    const std::string path = transformPath ? transformPath : "";
    const size_t      dot  = path.find('.');
    if (dot == std::string::npos)
    {
        return false;
    }

    const std::string property  = path.substr(0, dot);
    const std::string component = path.substr(dot + 1);

    return _sceneManager->editSceneGraph(
        [&](CpuScene& scene)
        {
            CpuSceneNode* node = scene.getNode(nodeID);
            if (!node)
            {
                return false;
            }

            CpuSceneNodeTransform local = node->local;
            if (property == "translation")
            {
                if (!setVectorComponent(local.translation, component, componentValue))
                {
                    return false;
                }
                scene.setLocalTranslation(nodeID, local.translation);
                return true;
            }

            if (property == "rotation")
            {
                if (!setVectorComponent(local.rotation, component, degreesToRadians(componentValue)))
                {
                    return false;
                }
                scene.setLocalRotation(nodeID, local.rotation);
                return true;
            }

            if (property == "scale")
            {
                if (!setVectorComponent(local.scale, component, componentValue))
                {
                    return false;
                }
                scene.setLocalScale(nodeID, local.scale);
                return true;
            }

            return false;
        });
}

bool SceneManagerEditor::addSceneNodeComponent(const char* nodeKey, const char* componentType)
{
    if (!_sceneManager || !isSameText(componentType, "ModelComponent"))
    {
        return false;
    }

    CpuSceneNodeID nodeID;
    if (!parseNodeKey(nodeKey, nodeID))
    {
        return false;
    }

    return _sceneManager->editSceneGraph(
        [&](CpuScene& scene)
        {
            CpuSceneNode* node = scene.getNode(nodeID);
            if (!node || node->type != CpuSceneNodeType::eNode3D)
            {
                return false;
            }

            if (scene.getComponent<CpuModelComponent>(nodeID))
            {
                return true;
            }

            return scene.addComponent<CpuModelComponent>(nodeID) != nullptr;
        });
}

} // namespace Play::editor
