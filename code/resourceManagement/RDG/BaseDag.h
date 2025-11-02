#ifndef RDG_BASE_DAG_H
#define RDG_BASE_DAG_H

#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>

namespace Play::RDG
{

// 前向声明
class Dag;
class Edge;

// EdgeType可以保留，用于给边附加语义，但它不再影响图的拓扑结构
enum class EdgeType
{
    eGeneral,
    eTexture,
    eBuffer,
    eRenderAttachment
};

enum class NodeType
{
    eGeneral,
    eRenderPass,
    eComputePass,
    eRTPass,
    eInput,
    eOutput
};

// template <NodePriority P = NodePriority::eSecondary>
class Node
{
public:
    virtual ~Node() = default;

    size_t getId() const
    {
        return m_id;
    }
    const std::vector<Edge*>& getIncomingEdges() const
    {
        return m_incoming_edges;
    }
    const std::vector<Edge*>& getOutgoingEdges() const
    {
        return m_outgoing_edges;
    }

    bool isCull() const
    {
        return m_is_cull;
    }

    void setCull(bool cull)
    {
        m_is_cull = cull;
    }

protected:
    Node(size_t id, NodeType type = NodeType::eGeneral) : m_id(id), m_type(type), m_is_cull(true) {}

private:
    friend class Dag;
    friend class Edge;
    size_t             m_id;
    NodeType           m_type;
    bool               m_is_cull = true; // 默认可以被剔除
    std::vector<Edge*> m_incoming_edges;
    std::vector<Edge*> m_outgoing_edges;
};

// Edge: 非模板基类，移除了isVirtual逻辑
class Edge
{
public:
    virtual ~Edge() = default;

    Node* getFrom() const
    {
        return m_from;
    }
    Node* getTo() const
    {
        return m_to;
    }
    EdgeType getType() const
    {
        return m_type;
    }

    // 构造函数简化
    Edge(Node* from, Node* to, EdgeType type = EdgeType::eGeneral)
        : m_from(from), m_to(to), m_type(type)
    {
    }

private:
    friend class Dag;
    Node*    m_from;
    Node*    m_to;
    EdgeType m_type;
};

class OutputNode : public Node
{
public:
    OutputNode(size_t id) : Node(id) {}
};

// Dag: 非模板类，内部逻辑简化
class Dag
{
public:
    Dag() = default;
    ~Dag();

    Dag(const Dag&)            = delete;
    Dag& operator=(const Dag&) = delete;

    template <typename T, typename... Args>
    T* addNode(Args&&... args)
    {
        static_assert(std::is_base_of<Node, T>::value, "T must be a subclass of Node");
        size_t id       = m_nodes.size();
        auto   node     = std::make_unique<T>(id, std::forward<Args>(args)...);
        T*     node_ptr = node.get();
        m_nodes.push_back(std::move(node));
        return node_ptr;
    }

    // template <typename... Args>
    Edge* createEdge(Node* from, Node* to)
    {
        auto  edge     = std::make_unique<Edge>(from, to);
        Edge* edge_ptr = edge.get();
        m_edges.push_back(std::move(edge));
        link(from, to, edge_ptr);
        return edge_ptr;
    }

    // link函数简化，不再需要处理虚拟边
    void link(Node* from, Node* to, Edge* edge);

    const std::vector<std::unique_ptr<Node>>& getNodes() const
    {
        return m_nodes;
    }

    // topologicalSort简化，不再需要处理虚拟边
    std::vector<Node*> topologicalSort();

    void clear();

    // 环路检测接口
    bool                            detectCycles() const;
    std::vector<std::vector<Node*>> findAllCycles() const;
    bool                            validate() const;

    // 节点剔除接口 - 基于优先级和依赖关系分析
    void culling(const std::vector<Node*>& outputNodes);

private:
    // pathExists简化，不再需要处理虚拟边
    bool pathExists(Node* start, Node* end);

    // 环路检测辅助函数
    bool hasCycleDFS(Node* node, std::unordered_map<Node*, int>& colors, std::vector<Node*>& path,
                     std::vector<std::vector<Node*>>& cycles) const;

    // 剔除分析辅助函数
    void markNeededNodesFromOutput(Node* outputNode, std::unordered_map<Node*, bool>& needed) const;
    Node* findClosestPrimaryInput(Node* secondaryNode, Node* outputNode) const;

    // 获取节点的逻辑依赖关系（用于环路检测）
    std::vector<Node*> getLogicalDependencies(Node* node) const;

    std::vector<std::unique_ptr<Node>> m_nodes;
    std::vector<std::unique_ptr<Edge>> m_edges;
};

} // namespace Play::RDG

#endif // RDG_BASE_DAG_H