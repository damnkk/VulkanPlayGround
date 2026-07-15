#include "CpuSceneSerialization.h"

#include "tinygltf/json.hpp"
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QUuid>
#include <nvutils/file_operations.hpp>

namespace Play
{

namespace
{
constexpr uint32_t    kSceneArchiveVersion    = 3;
constexpr uint32_t    kFirstSceneArchiveVersion = 1;
constexpr uint32_t    kMaxSceneHierarchyDepth = 1024;
constexpr const char* kProjectExtension       = ".project";
constexpr const char* kSceneDocumentName      = "scene.json";
constexpr const char* kAssetDocumentName      = "assets.json";

struct ProjectArchivePaths
{
    std::filesystem::path root;
    std::filesystem::path sceneDocument;
    std::filesystem::path assetDocument;
};

struct SerializedSceneNode
{
    std::string                      name;
    CpuSceneNodeType                 type = CpuSceneNodeType::eNode3D;
    CpuSceneNodeTransform            local;
    bool                             visible           = true;
    bool                             hasModelComponent = false;
    std::string                      modelAssetGuid;
    std::vector<SerializedSceneNode> children;
};

std::string makePersistedAssetPath(const ProjectAssetRecord& asset, const ProjectArchivePaths& paths)
{
    const std::filesystem::path assetPath = nvutils::pathFromUtf8(asset.sourcePath);
    if (assetPath.empty() || assetPath.is_relative())
    {
        return asset.sourcePath;
    }

    std::error_code       errorCode;
    std::filesystem::path relativePath = std::filesystem::relative(assetPath, paths.root, errorCode);
    if (errorCode || relativePath.empty())
    {
        return asset.sourcePath;
    }

    for (const std::filesystem::path& part : relativePath)
    {
        if (part == "..")
        {
            return asset.sourcePath;
        }
    }

    const std::string persistedPath = nvutils::utf8FromPath(relativePath);
    return persistedPath.empty() ? asset.sourcePath : persistedPath;
}

bool fail(std::string* errorMessage, const std::string& message)
{
    if (errorMessage)
    {
        *errorMessage = message;
    }
    return false;
}

bool resolveProjectPaths(const std::string& projectPath, bool appendMissingExtension, ProjectArchivePaths& output, std::string* errorMessage)
{
    if (projectPath.empty())
    {
        return fail(errorMessage, "Project path must not be empty.");
    }

    std::filesystem::path root = nvutils::pathFromUtf8(projectPath);
    if (root.empty())
    {
        return fail(errorMessage, "Project path is not valid UTF-8: " + projectPath);
    }

    if (root.extension().empty() && appendMissingExtension)
    {
        root += kProjectExtension;
    }

    if (root.extension() != kProjectExtension)
    {
        return fail(errorMessage, "Project path must use the .project extension.");
    }

    output.root          = root;
    output.sceneDocument = root / kSceneDocumentName;
    output.assetDocument = root / kAssetDocumentName;
    return true;
}

bool readJsonFile(const std::string& path, nlohmann::json& output, std::string* errorMessage)
{
    const std::string contents = nvutils::loadFile(path);
    if (contents.empty())
    {
        return fail(errorMessage, "Could not read JSON file: " + path);
    }

    output = nlohmann::json::parse(contents, nullptr, false);
    if (output.is_discarded())
    {
        return fail(errorMessage, "Invalid JSON file: " + path);
    }

    return true;
}

bool writeJsonFile(const std::string& path, const nlohmann::json& document, std::string* errorMessage)
{
    const QString filePath = QString::fromUtf8(path.c_str(), static_cast<qsizetype>(path.size()));
    QFile         file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return fail(errorMessage, "Could not open JSON file for writing: " + path);
    }

    const QByteArray contents = QByteArray::fromStdString(document.dump(2) + "\n");
    if (file.write(contents) != contents.size())
    {
        return fail(errorMessage, "Could not write JSON file: " + path);
    }

    return true;
}

bool readVersion(const nlohmann::json& document, const std::string& documentName, std::string* errorMessage)
{
    if (!document.is_object() || !document.contains("schemaVersion") ||
        (!document["schemaVersion"].is_number_integer() && !document["schemaVersion"].is_number_unsigned()) ||
        document["schemaVersion"] < kFirstSceneArchiveVersion || document["schemaVersion"] > kSceneArchiveVersion)
    {
        return fail(errorMessage, documentName + " must contain a supported schemaVersion.");
    }

    return true;
}

bool readRequiredString(const nlohmann::json& object, const char* key, std::string& output, const std::string& context, std::string* errorMessage)
{
    if (!object.contains(key) || !object[key].is_string())
    {
        return fail(errorMessage, context + " is missing string field '" + key + "'.");
    }

    output = object[key].get<std::string>();
    if (output.empty())
    {
        return fail(errorMessage, context + " field '" + key + "' must not be empty.");
    }

    return true;
}

bool readVec3(const nlohmann::json& object, const char* key, glm::vec3& output, const std::string& context, std::string* errorMessage)
{
    if (!object.contains(key) || !object[key].is_array() || object[key].size() != 3)
    {
        return fail(errorMessage, context + " must contain a three-element '" + key + "' array.");
    }

    const nlohmann::json& values = object[key];
    for (size_t index = 0; index < 3; ++index)
    {
        if (!values[index].is_number())
        {
            return fail(errorMessage, context + " field '" + key + "' must contain numbers.");
        }
    }

    output.x = values[0].get<float>();
    output.y = values[1].get<float>();
    output.z = values[2].get<float>();
    return true;
}

nlohmann::json writeVec3(const glm::vec3& value)
{
    return nlohmann::json::array({value.x, value.y, value.z});
}

const char* toAssetTypeName(ProjectAssetType type)
{
    switch (type)
    {
        case ProjectAssetType::eModel:
        default:
            return "model";
    }
}

bool parseAssetType(const std::string& name, ProjectAssetType& output)
{
    if (name == "model")
    {
        output = ProjectAssetType::eModel;
        return true;
    }

    return false;
}

const char* toNodeTypeName(CpuSceneNodeType type)
{
    return type == CpuSceneNodeType::eNode2D ? "2d" : "3d";
}

bool parseNodeType(const std::string& name, CpuSceneNodeType& output)
{
    if (name == "2d")
    {
        output = CpuSceneNodeType::eNode2D;
        return true;
    }

    if (name == "3d")
    {
        output = CpuSceneNodeType::eNode3D;
        return true;
    }

    return false;
}

bool parseAssetTable(const nlohmann::json& document, ProjectAssetTable& output, std::string* errorMessage)
{
    if (!readVersion(document, "Asset table", errorMessage))
    {
        return false;
    }

    if (!document.contains("assets") || !document["assets"].is_object())
    {
        return fail(errorMessage, "Asset table must contain an object field named 'assets'.");
    }

    ProjectAssetTable     parsedAssets;
    const nlohmann::json& assetEntries = document["assets"];
    for (auto it = assetEntries.begin(); it != assetEntries.end(); ++it)
    {
        const std::string     guid    = it.key();
        const nlohmann::json& entry   = it.value();
        const std::string     context = "Asset '" + guid + "'";
        if (guid.empty() || !entry.is_object())
        {
            return fail(errorMessage, context + " must be an object with a non-empty GUID key.");
        }

        std::string typeName;
        std::string sourcePath;
        const char* assetPathField = entry.contains("sourcePath") ? "sourcePath" : "binaryPath";
        if (!readRequiredString(entry, "type", typeName, context, errorMessage) ||
            !readRequiredString(entry, assetPathField, sourcePath, context, errorMessage))
        {
            return false;
        }

        ProjectAssetRecord record;
        record.guid       = guid;
        record.sourcePath = sourcePath;
        if (!parseAssetType(typeName, record.type) || !parsedAssets.set(record))
        {
            return fail(errorMessage, context + " has an unsupported type or invalid record.");
        }
    }

    output = parsedAssets;
    return true;
}

bool parseSceneNode(const nlohmann::json& input, const ProjectAssetTable& assets, SerializedSceneNode& output, uint32_t depth,
                    const std::string& context, std::string* errorMessage)
{
    if (depth > kMaxSceneHierarchyDepth)
    {
        return fail(errorMessage, "Scene hierarchy exceeds the maximum supported depth.");
    }

    if (!input.is_object())
    {
        return fail(errorMessage, context + " must be an object.");
    }

    if (!input.contains("name") || !input["name"].is_string())
    {
        return fail(errorMessage, context + " is missing string field 'name'.");
    }
    output.name = input["name"].get<std::string>();

    std::string typeName;
    if (!readRequiredString(input, "type", typeName, context, errorMessage))
    {
        return false;
    }

    if (!parseNodeType(typeName, output.type))
    {
        return fail(errorMessage, context + " has unsupported node type '" + typeName + "'.");
    }

    if (!input.contains("visible") || !input["visible"].is_boolean())
    {
        return fail(errorMessage, context + " is missing boolean field 'visible'.");
    }
    output.visible = input["visible"].get<bool>();

    if (!input.contains("local") || !input["local"].is_object())
    {
        return fail(errorMessage, context + " is missing object field 'local'.");
    }

    const nlohmann::json& local = input["local"];
    if (!readVec3(local, "translation", output.local.translation, context + " local transform", errorMessage) ||
        !readVec3(local, "rotationRadians", output.local.rotation, context + " local transform", errorMessage) ||
        !readVec3(local, "scale", output.local.scale, context + " local transform", errorMessage))
    {
        return false;
    }

    if (!input.contains("components") || !input["components"].is_array())
    {
        return fail(errorMessage, context + " is missing array field 'components'.");
    }

    output.hasModelComponent = false;
    for (const nlohmann::json& component : input["components"])
    {
        if (!component.is_object())
        {
            return fail(errorMessage, context + " contains a component that is not an object.");
        }

        std::string componentType;
        if (!readRequiredString(component, "type", componentType, context + " component", errorMessage))
        {
            return false;
        }

        if (componentType != "model" || output.hasModelComponent || output.type != CpuSceneNodeType::eNode3D)
        {
            return fail(errorMessage, context + " contains an unsupported component.");
        }

        if (!readRequiredString(component, "assetGuid", output.modelAssetGuid, context + " model component", errorMessage))
        {
            return false;
        }

        const ProjectAssetRecord* asset = assets.find(output.modelAssetGuid);
        if (!asset || asset->type != ProjectAssetType::eModel)
        {
            return fail(errorMessage, context + " references unknown model asset GUID '" + output.modelAssetGuid + "'.");
        }

        output.hasModelComponent = true;
    }

    if (!input.contains("children") || !input["children"].is_array())
    {
        return fail(errorMessage, context + " is missing array field 'children'.");
    }

    output.children.clear();
    for (size_t index = 0; index < input["children"].size(); ++index)
    {
        SerializedSceneNode child;
        if (!parseSceneNode(input["children"][index], assets, child, depth + 1, context + "/children[" + std::to_string(index) + "]", errorMessage))
        {
            return false;
        }
        output.children.push_back(child);
    }

    return true;
}

bool parseScene(const nlohmann::json& document, const ProjectAssetTable& assets, SerializedSceneNode& root, bool& hasSerializedRoot,
                std::vector<SerializedSceneNode>& output, std::string* errorMessage)
{
    if (!readVersion(document, "Scene", errorMessage))
    {
        return false;
    }

    hasSerializedRoot = document.contains("root");
    if (hasSerializedRoot)
    {
        if (!parseSceneNode(document["root"], assets, root, 0, "Scene/root", errorMessage))
        {
            return false;
        }

        if (root.type != CpuSceneNodeType::eNode3D)
        {
            return fail(errorMessage, "Scene root must be a 3D node.");
        }

        output.clear();
        return true;
    }

    if (!document.contains("nodes") || !document["nodes"].is_array())
    {
        return fail(errorMessage, "Scene must contain an array field named 'nodes'.");
    }

    output.clear();
    for (size_t index = 0; index < document["nodes"].size(); ++index)
    {
        SerializedSceneNode node;
        if (!parseSceneNode(document["nodes"][index], assets, node, 0, "Scene/nodes[" + std::to_string(index) + "]", errorMessage))
        {
            return false;
        }
        output.push_back(node);
    }

    return true;
}

bool writeSceneNode(const CpuScene& scene, CpuSceneNodeID nodeID, const ProjectAssetTable& assets, nlohmann::json& output, std::string* errorMessage)
{
    const CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return fail(errorMessage, "Scene contains an invalid node.");
    }

    output            = nlohmann::json::object();
    output["name"]    = node->name;
    output["type"]    = toNodeTypeName(node->type);
    output["visible"] = node->visible;
    output["local"]   = {
        {"translation", writeVec3(node->local.translation)},
        {"rotationRadians", writeVec3(node->local.rotation)},
        {"scale", writeVec3(node->local.scale)},
    };
    output["components"] = nlohmann::json::array();

    const CpuModelComponent* modelComponent = scene.getComponent<CpuModelComponent>(nodeID);
    if (modelComponent && !modelComponent->assetGuid.empty())
    {
        const ProjectAssetRecord* asset = assets.find(modelComponent->assetGuid);
        if (!asset || asset->type != ProjectAssetType::eModel)
        {
            return fail(errorMessage,
                        "Model component on node '" + node->name + "' references unknown asset GUID '" + modelComponent->assetGuid + "'.");
        }

        output["components"].push_back({{"type", "model"}, {"assetGuid", modelComponent->assetGuid}});
    }

    output["children"]     = nlohmann::json::array();
    CpuSceneNodeID childID = node->firstChild;
    while (scene.isValid(childID))
    {
        const CpuSceneNode*  child       = scene.getNode(childID);
        const CpuSceneNodeID nextChildID = child ? child->nextSibling : CpuSceneNodeID{};

        nlohmann::json childOutput;
        if (!writeSceneNode(scene, childID, assets, childOutput, errorMessage))
        {
            return false;
        }
        output["children"].push_back(childOutput);
        childID = nextChildID;
    }

    return true;
}

void appendSceneNode(CpuScene& scene, CpuSceneNodeID parentNodeID, const SerializedSceneNode& input);

void applySceneNode(CpuScene& scene, CpuSceneNodeID nodeID, const SerializedSceneNode& input)
{
    CpuSceneNode* node = scene.getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->name = input.name;
    scene.setVisible(nodeID, input.visible);
    scene.setLocalTransform(nodeID, input.local);

    if (input.hasModelComponent)
    {
        CpuModelComponent* component = scene.addComponent<CpuModelComponent>(nodeID);
        if (component)
        {
            component->assetGuid = input.modelAssetGuid;
        }
    }

    // CpuScene prepends children. Building in reverse retains the serialized order.
    for (auto it = input.children.rbegin(); it != input.children.rend(); ++it)
    {
        appendSceneNode(scene, nodeID, *it);
    }
}

void appendSceneNode(CpuScene& scene, CpuSceneNodeID parentNodeID, const SerializedSceneNode& input)
{
    const CpuSceneNodeID nodeID =
        input.type == CpuSceneNodeType::eNode2D ? scene.create2DNode(input.name, parentNodeID) : scene.create3DNode(input.name, parentNodeID);
    applySceneNode(scene, nodeID, input);
}
} // namespace

bool ProjectAssetTable::set(const ProjectAssetRecord& record)
{
    if (record.guid.empty() || record.sourcePath.empty())
    {
        return false;
    }

    for (ProjectAssetRecord& existing : _records)
    {
        if (existing.guid == record.guid)
        {
            existing = record;
            return true;
        }
    }

    _records.push_back(record);
    return true;
}

void ProjectAssetTable::clear()
{
    _records.clear();
}

const ProjectAssetRecord* ProjectAssetTable::find(const std::string& guid) const
{
    for (const ProjectAssetRecord& record : _records)
    {
        if (record.guid == guid)
        {
            return &record;
        }
    }

    return nullptr;
}

std::string generateProjectAssetGuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

bool createProjectArchive(const std::string& projectPath, std::string* errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    ProjectArchivePaths paths;
    if (!resolveProjectPaths(projectPath, true, paths, errorMessage))
    {
        return false;
    }

    std::error_code errorCode;
    if (std::filesystem::exists(paths.root, errorCode))
    {
        return fail(errorMessage, "Project already exists: " + nvutils::utf8FromPath(paths.root));
    }

    if (errorCode || !std::filesystem::create_directories(paths.root, errorCode) || errorCode)
    {
        return fail(errorMessage, "Could not create project directory: " + nvutils::utf8FromPath(paths.root));
    }

    CpuScene          emptyScene;
    ProjectAssetTable emptyAssets;
    return saveProjectArchive(emptyScene, emptyAssets, nvutils::utf8FromPath(paths.root), errorMessage);
}

bool saveProjectArchive(const CpuScene& scene, const ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    ProjectArchivePaths paths;
    if (!resolveProjectPaths(projectPath, false, paths, errorMessage))
    {
        return false;
    }

    std::error_code errorCode;
    if (!std::filesystem::is_directory(paths.root, errorCode) || errorCode)
    {
        return fail(errorMessage, "Project directory does not exist: " + nvutils::utf8FromPath(paths.root));
    }

    nlohmann::json assetDocument;
    assetDocument["schemaVersion"] = kSceneArchiveVersion;
    assetDocument["assets"]        = nlohmann::json::object();
    for (const ProjectAssetRecord& asset : assets.getRecords())
    {
        if (asset.guid.empty() || asset.sourcePath.empty())
        {
            return fail(errorMessage, "Asset table contains an invalid record.");
        }

        const std::string persistedAssetPath = makePersistedAssetPath(asset, paths);
        assetDocument["assets"][asset.guid] = {
            {"type", toAssetTypeName(asset.type)},
            {"sourcePath", persistedAssetPath},
        };
    }

    nlohmann::json sceneDocument;
    sceneDocument["schemaVersion"] = kSceneArchiveVersion;

    const CpuSceneNode* root = scene.getNode(scene.rootNode());
    if (!root)
    {
        return fail(errorMessage, "CPU scene has no valid root node.");
    }

    if (!writeSceneNode(scene, scene.rootNode(), assets, sceneDocument["root"], errorMessage))
    {
        return false;
    }

    const std::string assetDocumentPath = nvutils::utf8FromPath(paths.assetDocument);
    const std::string sceneDocumentPath = nvutils::utf8FromPath(paths.sceneDocument);
    if (assetDocumentPath.empty() || sceneDocumentPath.empty())
    {
        return fail(errorMessage, "Project contains a path that cannot be represented as UTF-8.");
    }

    if (!writeJsonFile(assetDocumentPath, assetDocument, errorMessage))
    {
        return false;
    }

    return writeJsonFile(sceneDocumentPath, sceneDocument, errorMessage);
}

bool loadProjectArchive(CpuScene& scene, ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    ProjectArchivePaths paths;
    if (!resolveProjectPaths(projectPath, false, paths, errorMessage))
    {
        return false;
    }

    std::error_code errorCode;
    if (!std::filesystem::is_directory(paths.root, errorCode) || errorCode)
    {
        return fail(errorMessage, "Project directory does not exist: " + nvutils::utf8FromPath(paths.root));
    }

    const std::string sceneDocumentPath = nvutils::utf8FromPath(paths.sceneDocument);
    const std::string assetDocumentPath = nvutils::utf8FromPath(paths.assetDocument);
    if (sceneDocumentPath.empty() || assetDocumentPath.empty())
    {
        return fail(errorMessage, "Project contains a path that cannot be represented as UTF-8.");
    }

    nlohmann::json sceneDocument;
    nlohmann::json assetDocument;
    if (!readJsonFile(sceneDocumentPath, sceneDocument, errorMessage) || !readJsonFile(assetDocumentPath, assetDocument, errorMessage))
    {
        return false;
    }

    ProjectAssetTable parsedAssets;
    if (!parseAssetTable(assetDocument, parsedAssets, errorMessage))
    {
        return false;
    }

    SerializedSceneNode              parsedRoot;
    bool                             hasSerializedRoot = false;
    std::vector<SerializedSceneNode> parsedNodes;
    if (!parseScene(sceneDocument, parsedAssets, parsedRoot, hasSerializedRoot, parsedNodes, errorMessage))
    {
        return false;
    }

    scene.clear();
    assets = parsedAssets;
    if (hasSerializedRoot)
    {
        applySceneNode(scene, scene.rootNode(), parsedRoot);
    }
    else
    {
        for (auto it = parsedNodes.rbegin(); it != parsedNodes.rend(); ++it)
        {
            appendSceneNode(scene, scene.rootNode(), *it);
        }
    }
    scene.updateWorldTransforms();
    return true;
}

} // namespace Play
