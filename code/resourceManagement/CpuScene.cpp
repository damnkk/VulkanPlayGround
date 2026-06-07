#include "CpuScene.h"
#include "AssetLoadingServer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Play
{

namespace
{
glm::mat4 composeLocalTransform(const CpuSceneNodeTransform& transform)
{
    glm::mat4 matrix = glm::translate(glm::mat4(1.0f), transform.translation);
    matrix *= glm::mat4_cast(glm::quat(transform.rotation));
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

CpuSceneNodeTransform decomposeLocalTransform(const glm::mat4& matrix)
{
    CpuSceneNodeTransform transform;
    transform.translation = glm::vec3(matrix[3]);

    glm::vec3 axisX = glm::vec3(matrix[0]);
    glm::vec3 axisY = glm::vec3(matrix[1]);
    glm::vec3 axisZ = glm::vec3(matrix[2]);

    transform.scale.x = glm::length(axisX);
    transform.scale.y = glm::length(axisY);
    transform.scale.z = glm::length(axisZ);

    if (glm::determinant(glm::mat3(matrix)) < 0.0f)
    {
        transform.scale.x = -transform.scale.x;
    }

    if (transform.scale.x != 0.0f)
    {
        axisX /= transform.scale.x;
    }
    if (transform.scale.y != 0.0f)
    {
        axisY /= transform.scale.y;
    }
    if (transform.scale.z != 0.0f)
    {
        axisZ /= transform.scale.z;
    }

    glm::mat3 rotationMatrix(1.0f);
    rotationMatrix[0]   = axisX;
    rotationMatrix[1]   = axisY;
    rotationMatrix[2]   = axisZ;
    transform.rotation  = glm::eulerAngles(glm::quat_cast(rotationMatrix));
    return transform;
}
} // namespace

ModelLoadRequestID CpuModelComponent::requestLoadFromFile(CpuScene& scene, AssetLoadingServer& loadingServer, const std::string& path,
                                                          const ModelLoadingConfig& loadingCfg)
{
    sourcePath      = path;
    loadingConfig   = loadingCfg;
    request         = loadingServer.requestModelLoad(self, path, loadingCfg);
    model           = {};
    firstRenderable = 0;
    renderableCount = INVALID_SCENE_ID;
    loadState       = request.isValid() ? LoadState::eQueued : LoadState::eFailed;
    loadMessage.clear();

    scene.notifyComponentChanged();
    return request;
}

ComponentStore::~ComponentStore()
{
    clear();
}

void ComponentStore::clear()
{
    for (PoolEntry& entry : _pools)
    {
        delete entry.pool;
        entry.pool = nullptr;
    }
    _pools.clear();
}

void ComponentStore::remove(CpuSceneComponentID componentID)
{
    IComponentPool* pool = findPool(componentID.typeID);
    if (!pool)
    {
        return;
    }
    pool->remove(componentID.index, componentID.generation);
}

CpuSceneComponent* ComponentStore::get(CpuSceneComponentID componentID)
{
    IComponentPool* pool = findPool(componentID.typeID);
    if (!pool)
    {
        return nullptr;
    }
    return pool->get(componentID.index, componentID.generation);
}

const CpuSceneComponent* ComponentStore::get(CpuSceneComponentID componentID) const
{
    const IComponentPool* pool = findPool(componentID.typeID);
    if (!pool)
    {
        return nullptr;
    }
    return pool->get(componentID.index, componentID.generation);
}

uint32_t ComponentStore::allocateComponentTypeID()
{
    static uint32_t nextTypeID = 0;
    return nextTypeID++;
}

ComponentStore::IComponentPool* ComponentStore::findPool(uint32_t typeID)
{
    for (PoolEntry& entry : _pools)
    {
        if (entry.typeID == typeID)
        {
            return entry.pool;
        }
    }
    return nullptr;
}

const ComponentStore::IComponentPool* ComponentStore::findPool(uint32_t typeID) const
{
    for (const PoolEntry& entry : _pools)
    {
        if (entry.typeID == typeID)
        {
            return entry.pool;
        }
    }
    return nullptr;
}

CpuScene::CpuScene()
{
    clear();
}

void CpuScene::clear()
{
    _nodes.clear();
    _freeNodeSlots.clear();
    _components.clear();
    CpuSceneNode rootNode;
    rootNode.name       = "Scene";
    rootNode.generation = 1;
    _nodes.push_back(rootNode);
    _rootNode       = makeNodeID(0);
    _revision       = 1;
    _transformDirty = true;
}

CpuSceneNodeID CpuScene::create2DNode(const std::string& name, CpuSceneNodeID parent)
{
    return createNode(name, parent, CpuSceneNodeType::eNode2D);
}

CpuSceneNodeID CpuScene::create3DNode(const std::string& name, CpuSceneNodeID parent)
{
    return createNode(name, parent, CpuSceneNodeType::eNode3D);
}

CpuSceneNodeID CpuScene::createNode(const std::string& name, CpuSceneNodeID parent, CpuSceneNodeType type)
{
    if (!isValid(parent))
    {
        parent = _rootNode;
    }

    CpuSceneNode node;
    node.name       = name;
    node.parent     = parent;
    node.type       = type;
    uint32_t nodeIndex = INVALID_SCENE_ID;
    if (!_freeNodeSlots.empty())
    {
        nodeIndex = _freeNodeSlots.back();
        _freeNodeSlots.pop_back();
        node.generation = _nodes[nodeIndex].generation;
        _nodes[nodeIndex] = node;
    }
    else
    {
        nodeIndex = static_cast<uint32_t>(_nodes.size());
        node.generation = 1;
        _nodes.push_back(node);
    }

    CpuSceneNodeID nodeID = makeNodeID(nodeIndex);
    attachChild(parent, nodeID);
    markWorldTransformDirty(nodeID);
    markDirty();
    return nodeID;
}

bool CpuScene::isValid(CpuSceneNodeID nodeID) const
{
    return nodeID.index < _nodes.size() && _nodes[nodeID.index].alive && _nodes[nodeID.index].generation == nodeID.generation;
}

CpuSceneNode* CpuScene::getNode(CpuSceneNodeID nodeID)
{
    if (!isValid(nodeID))
    {
        return nullptr;
    }
    return &_nodes[nodeID.index];
}

const CpuSceneNode* CpuScene::getNode(CpuSceneNodeID nodeID) const
{
    if (!isValid(nodeID))
    {
        return nullptr;
    }
    return &_nodes[nodeID.index];
}

void CpuScene::setLocalTransform(CpuSceneNodeID nodeID, const glm::mat4& localTransform)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->localTransform = localTransform;
    node->local          = decomposeLocalTransform(localTransform);
    markWorldTransformDirty(nodeID);
    markDirty();
}

void CpuScene::setLocalTransform(CpuSceneNodeID nodeID, const CpuSceneNodeTransform& localTransform)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->local          = localTransform;
    node->localTransform = composeLocalTransform(localTransform);
    markWorldTransformDirty(nodeID);
    markDirty();
}

void CpuScene::setLocalTranslation(CpuSceneNodeID nodeID, const glm::vec3& translation)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->local.translation = translation;
    node->localTransform    = composeLocalTransform(node->local);
    markWorldTransformDirty(nodeID);
    markDirty();
}

void CpuScene::setLocalRotation(CpuSceneNodeID nodeID, const glm::vec3& rotation)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->local.rotation = rotation;
    node->localTransform = composeLocalTransform(node->local);
    markWorldTransformDirty(nodeID);
    markDirty();
}

void CpuScene::setLocalScale(CpuSceneNodeID nodeID, const glm::vec3& scale)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->local.scale    = scale;
    node->localTransform = composeLocalTransform(node->local);
    markWorldTransformDirty(nodeID);
    markDirty();
}

void CpuScene::setVisible(CpuSceneNodeID nodeID, bool visible)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->visible = visible;
    markWorldTransformDirty(nodeID);
    markDirty();
}

bool CpuScene::reparentNode(CpuSceneNodeID nodeID, CpuSceneNodeID newParent)
{
    if (!isValid(nodeID) || !isValid(newParent) || isSameNode(nodeID, _rootNode))
    {
        return false;
    }

    if (isDescendantOf(newParent, nodeID))
    {
        return false;
    }

    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return false;
    }

    if (isSameNode(node->parent, newParent))
    {
        return true;
    }

    CpuSceneNodeID oldParent = node->parent;
    if (!detachChild(oldParent, nodeID))
    {
        return false;
    }

    attachChild(newParent, nodeID);
    markWorldTransformDirty(nodeID);
    markDirty();
    return true;
}

bool CpuScene::removeNode(CpuSceneNodeID nodeID)
{
    if (!isValid(nodeID) || isSameNode(nodeID, _rootNode))
    {
        return false;
    }

    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return false;
    }

    CpuSceneNodeID parent = node->parent;
    if (!detachChild(parent, nodeID))
    {
        return false;
    }

    removeNodeRecursive(nodeID);
    markDirty();
    return true;
}

void CpuScene::updateWorldTransforms()
{
    if (!_transformDirty)
    {
        return;
    }

    updateWorldRecursive(_rootNode, glm::mat4(1.0f), true);
    _transformDirty = false;
}

void CpuScene::notifyComponentChanged()
{
    markDirty();
}

CpuSceneNodeID CpuScene::makeNodeID(uint32_t index) const
{
    CpuSceneNodeID id;
    id.index      = index;
    id.generation = _nodes[index].generation;
    return id;
}

void CpuScene::attachChild(CpuSceneNodeID parent, CpuSceneNodeID child)
{
    CpuSceneNode* parentNode = getNode(parent);
    CpuSceneNode* childNode  = getNode(child);
    if (!parentNode || !childNode)
    {
        return;
    }

    childNode->nextSibling = parentNode->firstChild;
    parentNode->firstChild = child;
    childNode->parent      = parent;
}

bool CpuScene::detachChild(CpuSceneNodeID parent, CpuSceneNodeID child)
{
    CpuSceneNode* parentNode = getNode(parent);
    CpuSceneNode* childNode  = getNode(child);
    if (!parentNode || !childNode)
    {
        return false;
    }

    CpuSceneNodeID* link = &parentNode->firstChild;
    while (isValid(*link))
    {
        CpuSceneNode& linkedNode = _nodes[link->index];
        if (isSameNode(*link, child))
        {
            *link                  = linkedNode.nextSibling;
            childNode->parent      = {};
            childNode->nextSibling = {};
            return true;
        }
        link = &linkedNode.nextSibling;
    }

    return false;
}

bool CpuScene::isSameNode(CpuSceneNodeID lhs, CpuSceneNodeID rhs) const
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

bool CpuScene::isDescendantOf(CpuSceneNodeID nodeID, CpuSceneNodeID ancestorID) const
{
    CpuSceneNodeID current = nodeID;
    while (isValid(current))
    {
        if (isSameNode(current, ancestorID))
        {
            return true;
        }
        current = _nodes[current.index].parent;
    }

    return false;
}

void CpuScene::markWorldTransformDirty(CpuSceneNodeID nodeID)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->worldTransformDirty = true;

    CpuSceneNodeID childID = node->firstChild;
    while (isValid(childID))
    {
        const CpuSceneNodeID nextID = _nodes[childID.index].nextSibling;
        markWorldTransformDirty(childID);
        childID = nextID;
    }
}

void CpuScene::removeNodeRecursive(CpuSceneNodeID nodeID)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    CpuSceneNodeID childID = node->firstChild;
    while (isValid(childID))
    {
        CpuSceneNodeID nextID = _nodes[childID.index].nextSibling;
        removeNodeRecursive(childID);
        childID = nextID;
    }

    for (CpuSceneComponentID componentID : node->components)
    {
        _components.remove(componentID);
    }

    node->components.clear();
    node->parent       = {};
    node->firstChild   = {};
    node->nextSibling  = {};
    node->alive        = false;
    node->visible      = false;
    node->worldVisible = false;
    node->worldTransformDirty = false;
    ++node->generation;
    _freeNodeSlots.push_back(nodeID.index);
}

void CpuScene::updateWorldRecursive(CpuSceneNodeID nodeID, const glm::mat4& parentTransform, bool parentVisible)
{
    CpuSceneNode* node = getNode(nodeID);
    if (!node)
    {
        return;
    }

    node->worldTransform = parentTransform * node->localTransform;
    const bool visible   = parentVisible && node->visible;
    node->worldVisible   = visible;
    node->worldTransformDirty = false;

    CpuSceneNodeID childID = node->firstChild;
    while (isValid(childID))
    {
        CpuSceneNodeID nextID = _nodes[childID.index].nextSibling;
        updateWorldRecursive(childID, node->worldTransform, visible);
        childID = nextID;
    }
}

void CpuScene::markDirty()
{
    _transformDirty = true;
    ++_revision;
}

} // namespace Play
