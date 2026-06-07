#include "editor/SceneManagerEditor.h"

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
    const std::string text       = key ? key : "";
    size_t            cursor     = 0;
    uint32_t          index      = INVALID_SCENE_ID;
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

void appendComponentSnapshot(EditorUiSceneNode& output, const CpuScene& scene, CpuSceneNodeID nodeID)
{
    const CpuModelComponent* modelComponent = scene.getComponent<CpuModelComponent>(nodeID);
    if (!modelComponent)
    {
        return;
    }

    EditorUiSceneComponent component;
    component.typeName = "ModelComponent";
    component.details.push_back(EditorUiKeyValue{"Path", modelComponent->sourcePath.empty() ? "-" : modelComponent->sourcePath});
    component.details.push_back(EditorUiKeyValue{"State", loadStateLabel(modelComponent->loadState)});
    component.details.push_back(
        EditorUiKeyValue{"Renderables", modelComponent->usesAllRenderables() ? "All" : std::to_string(modelComponent->renderableCount)});
    if (!modelComponent->loadMessage.empty())
    {
        component.details.push_back(EditorUiKeyValue{"Message", modelComponent->loadMessage});
    }
    output.components.push_back(component);
}

EditorUiSceneNode buildSceneNodeSnapshotRecursive(const CpuScene& scene, CpuSceneNodeID nodeID)
{
    EditorUiSceneNode output;

    const CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return output;
    }

    output.key      = makeNodeKey(nodeID);
    output.name     = node->name;
    output.typeName = nodeTypeLabel(node->type);
    output.is3D     = node->type == CpuSceneNodeType::eNode3D;

    output.transform.translation[0] = node->local.translation.x;
    output.transform.translation[1] = node->local.translation.y;
    output.transform.translation[2] = node->local.translation.z;
    output.transform.rotation[0]    = radiansToDegrees(node->local.rotation.x);
    output.transform.rotation[1]    = radiansToDegrees(node->local.rotation.y);
    output.transform.rotation[2]    = radiansToDegrees(node->local.rotation.z);
    output.transform.scale[0]       = node->local.scale.x;
    output.transform.scale[1]       = node->local.scale.y;
    output.transform.scale[2]       = node->local.scale.z;

    const bool hasModelComponent = scene.getComponent<CpuModelComponent>(nodeID) != nullptr;
    output.canAddModelComponent  = output.is3D && !hasModelComponent;
    appendComponentSnapshot(output, scene, nodeID);

    CpuSceneNodeID childID = node->firstChild;
    while (scene.isValid(childID))
    {
        const CpuSceneNode*  child  = scene.getNode(childID);
        const CpuSceneNodeID nextID = child ? child->nextSibling : CpuSceneNodeID{};
        output.children.push_back(buildSceneNodeSnapshotRecursive(scene, childID));
        childID = nextID;
    }

    return output;
}
} // namespace

SceneManagerEditor::SceneManagerEditor(const char*) {}

void SceneManagerEditor::setSceneManager(Play::SceneManager* sceneManager)
{
    _sceneManager = sceneManager;
}

void SceneManagerEditor::buildSnapshot(EditorUiRenderMode& renderMode) const
{
    if (!_sceneManager)
    {
        renderMode.scene.available = false;
        renderMode.scene.emptyText = "No SceneManager for this render mode.";
        return;
    }

    _sceneManager->readSceneGraph(
        [&](const CpuScene& scene)
        {
            renderMode.scene.available = true;
            renderMode.scene.emptyText.clear();
            renderMode.scene.root = buildSceneNodeSnapshotRecursive(scene, scene.rootNode());
        });
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

bool SceneManagerEditor::loadSceneNodeModel(const char* nodeKey, const char* path)
{
    if (!_sceneManager || !path || !path[0])
    {
        return false;
    }

    CpuSceneNodeID nodeID;
    if (!parseNodeKey(nodeKey, nodeID))
    {
        return false;
    }

    const std::string sourcePath = path;
    return _sceneManager->editSceneGraph(
        [&](CpuScene& scene)
        {
            CpuSceneNode* node = scene.getNode(nodeID);
            if (!node || node->type != CpuSceneNodeType::eNode3D)
            {
                return false;
            }

            CpuModelComponent* component = scene.getComponent<CpuModelComponent>(nodeID);
            if (!component)
            {
                component = scene.addComponent<CpuModelComponent>(nodeID);
            }
            if (!component)
            {
                return false;
            }

            const ModelLoadingConfig loadingConfig = component->loadingConfig;
            return _sceneManager->editAssetLoadingServer(
                [&](AssetLoadingServer& loadingServer)
                {
                    return component->requestLoadFromFile(scene, loadingServer, sourcePath, loadingConfig).isValid();
                });
        });
}

} // namespace Play::editor
