#ifndef CPU_SCENE_H
#define CPU_SCENE_H

#include "ModelLoadingConfig.h"
#include "pch.h"
#include <glm/glm.hpp>
#include <rttr/rttr_enable.h>
#include <string>
#include <vector>

namespace Play
{

class AssetLoadingServer;
class CpuScene;
struct ModelLoadResult;

constexpr uint32_t INVALID_SCENE_ID = ~0u;

struct ModelAssetID
{
    uint32_t index      = INVALID_SCENE_ID;
    uint32_t generation = 0;

    bool isValid() const
    {
        return index != INVALID_SCENE_ID;
    }
};

struct CpuSceneNodeID
{
    uint32_t index      = INVALID_SCENE_ID;
    uint32_t generation = 0;

    bool isValid() const
    {
        return index != INVALID_SCENE_ID;
    }
};

struct CpuSceneComponentID
{
    uint32_t typeID     = INVALID_SCENE_ID;
    uint32_t index      = INVALID_SCENE_ID;
    uint32_t generation = 0;

    bool isValid() const
    {
        return typeID != INVALID_SCENE_ID && index != INVALID_SCENE_ID;
    }
};

enum class CpuSceneNodeType : uint32_t
{
    eNode2D,
    eNode3D
};

class CpuSceneComponent
{
public:
    virtual ~CpuSceneComponent() = default;

    CpuSceneComponentID self;
    CpuSceneNodeID      ownerNode;
    uint32_t generation = 1;
    bool     visible    = true;

    RTTR_ENABLE()
};

class CpuModelComponent : public CpuSceneComponent
{
public:
    enum class LoadState : uint32_t
    {
        eEmpty,
        eQueued,
        eLoading,
        eLoaded,
        eFailed
    };

    std::string  sourcePath;
    ModelLoadingConfig loadingConfig;
    ModelLoadRequestID request;
    ModelAssetID model;
    uint32_t     firstRenderable = 0;
    uint32_t     renderableCount = INVALID_SCENE_ID;
    LoadState    loadState       = LoadState::eEmpty;
    std::string  loadMessage;

    ModelLoadRequestID requestLoadFromFile(CpuScene& scene, AssetLoadingServer& loadingServer, const std::string& path,
                                           const ModelLoadingConfig& loadingCfg);

    bool hasModel() const
    {
        return model.isValid();
    }

    bool usesAllRenderables() const
    {
        return renderableCount == INVALID_SCENE_ID;
    }

    RTTR_ENABLE(CpuSceneComponent)
};

class ComponentStore
{
    class IComponentPool;
    template <typename T>
    class ComponentPool;
    struct PoolEntry;

public:
    ComponentStore() = default;
    ComponentStore(const ComponentStore&) = delete;
    ComponentStore& operator=(const ComponentStore&) = delete;
    ~ComponentStore();

    void clear();
    void remove(CpuSceneComponentID componentID);

    template <typename T>
    CpuSceneComponentID create()
    {
        const uint32_t typeID = getComponentTypeID<T>();
        ComponentPool<T>* pool = getOrCreatePool<T>(typeID);
        return pool->create(typeID);
    }

    template <typename T>
    T* get(CpuSceneComponentID componentID)
    {
        if (componentID.typeID != getComponentTypeID<T>())
        {
            return nullptr;
        }

        IComponentPool* pool = findPool(componentID.typeID);
        if (!pool)
        {
            return nullptr;
        }

        return static_cast<ComponentPool<T>*>(pool)->getTyped(componentID.index, componentID.generation);
    }

    template <typename T>
    const T* get(CpuSceneComponentID componentID) const
    {
        if (componentID.typeID != getComponentTypeID<T>())
        {
            return nullptr;
        }

        const IComponentPool* pool = findPool(componentID.typeID);
        if (!pool)
        {
            return nullptr;
        }

        return static_cast<const ComponentPool<T>*>(pool)->getTyped(componentID.index, componentID.generation);
    }

    CpuSceneComponent*       get(CpuSceneComponentID componentID);
    const CpuSceneComponent* get(CpuSceneComponentID componentID) const;

    template <typename T>
    static uint32_t getComponentTypeID()
    {
        static const uint32_t typeID = allocateComponentTypeID();
        return typeID;
    }

private:
    class IComponentPool
    {
    public:
        virtual ~IComponentPool() = default;
        virtual CpuSceneComponent*       get(uint32_t index, uint32_t generation)       = 0;
        virtual const CpuSceneComponent* get(uint32_t index, uint32_t generation) const = 0;
        virtual void                     remove(uint32_t index, uint32_t generation)    = 0;
    };

    template <typename T>
    class ComponentPool : public IComponentPool
    {
    public:
        CpuSceneComponentID create(uint32_t typeID)
        {
            uint32_t generation = 1;
            uint32_t index      = INVALID_SCENE_ID;

            T component;
            if (!_freeSlots.empty())
            {
                index = _freeSlots.back();
                _freeSlots.pop_back();
                generation = _items[index].generation;
            }
            else
            {
                index = static_cast<uint32_t>(_items.size());
                _items.push_back(component);
            }

            component.generation = generation;
            _items[index]        = component;

            CpuSceneComponentID componentID;
            componentID.typeID     = typeID;
            componentID.index      = index;
            componentID.generation = _items[index].generation;
            _items[index].self     = componentID;
            return componentID;
        }

        T* getTyped(uint32_t index, uint32_t generation)
        {
            if (index >= _items.size() || _items[index].generation != generation)
            {
                return nullptr;
            }
            return &_items[index];
        }

        const T* getTyped(uint32_t index, uint32_t generation) const
        {
            if (index >= _items.size() || _items[index].generation != generation)
            {
                return nullptr;
            }
            return &_items[index];
        }

        CpuSceneComponent* get(uint32_t index, uint32_t generation) override
        {
            return getTyped(index, generation);
        }

        const CpuSceneComponent* get(uint32_t index, uint32_t generation) const override
        {
            return getTyped(index, generation);
        }

        void remove(uint32_t index, uint32_t generation) override
        {
            if (index >= _items.size() || _items[index].generation != generation)
            {
                return;
            }

            _items[index].visible = false;
            _items[index].self      = {};
            _items[index].ownerNode = {};
            ++_items[index].generation;
            _freeSlots.push_back(index);
        }

    private:
        std::vector<T> _items;
        std::vector<uint32_t> _freeSlots;
    };

    struct PoolEntry
    {
        uint32_t        typeID = INVALID_SCENE_ID;
        IComponentPool* pool   = nullptr;
    };

    static uint32_t allocateComponentTypeID();

    IComponentPool*       findPool(uint32_t typeID);
    const IComponentPool* findPool(uint32_t typeID) const;

    template <typename T>
    ComponentPool<T>* getOrCreatePool(uint32_t typeID)
    {
        IComponentPool* pool = findPool(typeID);
        if (pool)
        {
            return static_cast<ComponentPool<T>*>(pool);
        }

        PoolEntry entry;
        entry.typeID = typeID;
        entry.pool   = new ComponentPool<T>();
        _pools.push_back(entry);
        return static_cast<ComponentPool<T>*>(entry.pool);
    }

    std::vector<PoolEntry> _pools;
};

struct CpuSceneNode
{
    CpuSceneNodeType type = CpuSceneNodeType::eNode3D;

    std::string    name;
    CpuSceneNodeID parent;
    CpuSceneNodeID firstChild;
    CpuSceneNodeID nextSibling;

    glm::mat4 localTransform = glm::mat4(1.0f);
    glm::mat4 worldTransform = glm::mat4(1.0f);

    std::vector<CpuSceneComponentID> components;
    uint32_t                         generation   = 1;
    bool                             alive        = true;
    bool                             visible      = true;
    bool                             worldVisible = true;

    template <typename T>
    T* addComponent(ComponentStore& componentStore)
    {
        T* component = getComponent<T>(componentStore);
        if (component)
        {
            return component;
        }

        CpuSceneComponentID componentID = componentStore.create<T>();
        components.push_back(componentID);
        return componentStore.get<T>(componentID);
    }

    template <typename T>
    T* getComponent(ComponentStore& componentStore)
    {
        for (CpuSceneComponentID componentID : components)
        {
            T* component = componentStore.get<T>(componentID);
            if (component)
            {
                return component;
            }
        }
        return nullptr;
    }

    template <typename T>
    const T* getComponent(const ComponentStore& componentStore) const
    {
        for (CpuSceneComponentID componentID : components)
        {
            const T* component = componentStore.get<T>(componentID);
            if (component)
            {
                return component;
            }
        }
        return nullptr;
    }

    template <typename T>
    bool removeComponent(ComponentStore& componentStore)
    {
        for (auto it = components.begin(); it != components.end(); ++it)
        {
            T* component = componentStore.get<T>(*it);
            if (component)
            {
                componentStore.remove(*it);
                components.erase(it);
                return true;
            }
        }
        return false;
    }
};

class CpuScene
{
public:
    CpuScene();

    void clear();

    CpuSceneNodeID rootNode() const
    {
        return _rootNode;
    }

    CpuSceneNodeID create2DNode(const std::string& name, CpuSceneNodeID parent);
    CpuSceneNodeID create3DNode(const std::string& name, CpuSceneNodeID parent);

    bool isValid(CpuSceneNodeID nodeID) const;

    CpuSceneNode*       getNode(CpuSceneNodeID nodeID);
    const CpuSceneNode* getNode(CpuSceneNodeID nodeID) const;

    void setLocalTransform(CpuSceneNodeID nodeID, const glm::mat4& localTransform);
    void setVisible(CpuSceneNodeID nodeID, bool visible);
    bool reparentNode(CpuSceneNodeID nodeID, CpuSceneNodeID newParent);
    bool removeNode(CpuSceneNodeID nodeID);

    template <typename T>
    T* addComponent(CpuSceneNodeID nodeID)
    {
        CpuSceneNode* node = getNode(nodeID);
        if (!node)
        {
            return nullptr;
        }

        T* component = node->addComponent<T>(_components);
        if (component)
        {
            component->ownerNode = nodeID;
        }
        markDirty();
        return component;
    }

    template <typename T>
    T* getComponent(CpuSceneNodeID nodeID)
    {
        CpuSceneNode* node = getNode(nodeID);
        if (!node)
        {
            return nullptr;
        }
        return node->getComponent<T>(_components);
    }

    template <typename T>
    const T* getComponent(CpuSceneNodeID nodeID) const
    {
        const CpuSceneNode* node = getNode(nodeID);
        if (!node)
        {
            return nullptr;
        }
        return node->getComponent<T>(_components);
    }

    template <typename T>
    bool removeComponent(CpuSceneNodeID nodeID)
    {
        CpuSceneNode* node = getNode(nodeID);
        if (!node)
        {
            return false;
        }

        const bool removed = node->removeComponent<T>(_components);
        if (removed)
        {
            markDirty();
        }
        return removed;
    }

    template <typename T>
    T* getComponent(CpuSceneComponentID componentID)
    {
        return _components.get<T>(componentID);
    }

    template <typename T>
    const T* getComponent(CpuSceneComponentID componentID) const
    {
        return _components.get<T>(componentID);
    }

    void updateWorldTransforms();
    void notifyComponentChanged();

    uint64_t getRevision() const
    {
        return _revision;
    }

    const std::vector<CpuSceneNode>& getNodes() const
    {
        return _nodes;
    }

private:
    friend class CpuModelComponent;

    CpuSceneNodeID makeNodeID(uint32_t index) const;
    CpuSceneNodeID createNode(const std::string& name, CpuSceneNodeID parent, CpuSceneNodeType type);
    void           attachChild(CpuSceneNodeID parent, CpuSceneNodeID child);
    bool           detachChild(CpuSceneNodeID parent, CpuSceneNodeID child);
    bool           isSameNode(CpuSceneNodeID lhs, CpuSceneNodeID rhs) const;
    bool           isDescendantOf(CpuSceneNodeID nodeID, CpuSceneNodeID ancestorID) const;
    void           removeNodeRecursive(CpuSceneNodeID nodeID);
    void           updateWorldRecursive(CpuSceneNodeID nodeID, const glm::mat4& parentTransform, bool parentVisible);
    void           markDirty();

    std::vector<CpuSceneNode> _nodes;
    std::vector<uint32_t>     _freeNodeSlots;
    ComponentStore            _components;
    CpuSceneNodeID           _rootNode;
    uint64_t                 _revision       = 0;
    bool                     _transformDirty = true;
};

} // namespace Play

#endif // CPU_SCENE_H
