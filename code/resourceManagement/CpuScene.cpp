#include "CpuScene.h"
#include "AssetLoadingServer.h"

namespace Play
{

namespace
{
CpuSceneComponentID findModelComponentID(CpuScene& scene, CpuModelComponent* target)
{
    for (const CpuSceneNode& node : scene.getNodes())
    {
        if (!node.alive)
        {
            continue;
        }

        for (CpuSceneComponentID componentID : node.components)
        {
            if (scene.getComponent<CpuModelComponent>(componentID) == target)
            {
                return componentID;
            }
        }
    }
    return {};
}
} // namespace

ModelLoadRequestID CpuModelComponent::requestLoadFromFile(CpuScene& scene, AssetLoadingServer& loadingServer, const std::string& path,
                                                          const ModelLoadingConfig& loadingCfg)
{
    const CpuSceneComponentID componentID = findModelComponentID(scene, this);

    sourcePath      = path;
    loadingConfig   = loadingCfg;
    request         = loadingServer.requestModelLoad(componentID, path, loadingCfg);
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
