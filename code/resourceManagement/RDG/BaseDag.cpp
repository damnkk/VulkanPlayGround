#include "BaseDag.h"

#include <cassert>
#include <stdexcept>
#include <list>
#include <unordered_map>
namespace Play::RDG
{
Dag::~Dag()
{
    clear();
}

void Dag::link(Node* from, Node* to, Edge* edge)
{
    if (!from || !to || !edge)
    {
        throw std::invalid_argument("Node and Edge pointers cannot be null.");
    }

    // 总是进行环路检测
    if (pathExists(to, from))
    {
        throw std::runtime_error("Adding this edge would create a cycle.");
    }

    from->m_outgoing_edges.push_back(edge);
    to->m_incoming_edges.push_back(edge);
}

std::vector<Node*> Dag::topologicalSort()
{
    std::vector<Node*>             sorted_order;
    std::unordered_map<Node*, int> in_degree;
    std::list<Node*>               queue;

    for (const auto& node_ptr : m_nodes)
    {
        // 入度计算简化
        in_degree[node_ptr.get()] = node_ptr->getIncomingEdges().size();
        if (in_degree[node_ptr.get()] == 0)
        {
            queue.push_back(node_ptr.get());
        }
    }

    while (!queue.empty())
    {
        Node* u = queue.front();
        queue.pop_front();
        sorted_order.push_back(u);

        for (Edge* edge : u->getOutgoingEdges())
        {
            Node* v = edge->getTo();
            in_degree[v]--;
            if (in_degree[v] == 0)
            {
                queue.push_back(v);
            }
        }
    }

    if (sorted_order.size() != m_nodes.size())
    {
        throw std::runtime_error("Graph has a cycle, topological sort failed.");
    }

    return sorted_order;
}

void Dag::clear()
{
    m_edges.clear();
    m_nodes.clear();
}

bool Dag::pathExists(Node* start, Node* end)
{
    if (start == end) return true;

    std::list<Node*>                stack;
    std::unordered_map<Node*, bool> visited;

    stack.push_back(start);
    visited[start] = true;

    while (!stack.empty())
    {
        Node* current = stack.back();
        stack.pop_back();

        for (Edge* edge : current->getOutgoingEdges())
        {
            Node* neighbor = edge->getTo();
            if (neighbor == end)
            {
                return true;
            }
            if (!visited[neighbor])
            {
                visited[neighbor] = true;
                stack.push_back(neighbor);
            }
        }
    }
    return false;
}

bool Dag::detectCycles() const
{
    std::unordered_map<Node*, int> colors; // 0: 白色, 1: 灰色, 2: 黑色
    for (const auto& node_ptr : m_nodes)
    {
        colors[node_ptr.get()] = 0;
    }

    for (const auto& node_ptr : m_nodes)
    {
        Node* node = node_ptr.get();
        if (colors[node] == 0)
        {
            std::vector<Node*>              path;
            std::vector<std::vector<Node*>> dummy_cycles;
            if (hasCycleDFS(node, colors, path, dummy_cycles))
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::vector<Node*>> Dag::findAllCycles() const
{
    std::vector<std::vector<Node*>> cycles;
    std::unordered_map<Node*, int>  colors; // 0: 白色, 1: 灰色, 2: 黑色

    for (const auto& node_ptr : m_nodes)
    {
        colors[node_ptr.get()] = 0;
    }

    for (const auto& node_ptr : m_nodes)
    {
        Node* node = node_ptr.get();
        if (colors[node] == 0)
        {
            std::vector<Node*> path;
            hasCycleDFS(node, colors, path, cycles);
        }
    }

    return cycles;
}

bool Dag::validate() const
{
    // 检查基本完整性
    for (const auto& node_ptr : m_nodes)
    {
        Node* node = node_ptr.get();

        // 检查入边
        for (Edge* edge : node->getIncomingEdges())
        {
            if (edge->getTo() != node)
            {
                return false; // 入边的目标节点不是当前节点
            }
        }

        // 检查出边
        for (Edge* edge : node->getOutgoingEdges())
        {
            if (edge->getFrom() != node)
            {
                return false; // 出边的源节点不是当前节点
            }
        }
    }

    // 检查是否有环路
    if (detectCycles())
    {
        return false;
    }

    return true;
}

void Dag::culling(const std::vector<Node*>& outputNodes)
{
    // 所有节点默认为可剔除
    //  从每个输出节点开始，标记需要保留的节点
    std::unordered_map<Node*, bool> needed;
    for (Node* outputNode : outputNodes)
    {
        markNeededNodesFromOutput(outputNode, needed);
    }

    // 将需要的节点标记为不可剔除
    for (const auto& [node, isNeeded] : needed)
    {
        if (isNeeded)
        {
            node->setCull(false);
        }
    }
}

void Dag::markNeededNodesFromOutput(Node* outputNode, std::unordered_map<Node*, bool>& needed) const
{
    if (needed[outputNode]) return; // 已经处理过

    needed[outputNode] = true;
    for (Edge* edge : outputNode->getIncomingEdges())
    {
        Node* connectedNode = edge->getFrom();
        markNeededNodesFromOutput(connectedNode, needed);
    }
}

Node* Dag::findClosestPrimaryInput(Node* secondaryNode, Node* outputNode) const
{
    (void) secondaryNode;
    (void) outputNode;
    return nullptr;
}

std::vector<Node*> Dag::getLogicalDependencies(Node* node) const
{
    std::vector<Node*> dependencies;

    for (Edge* edge : node->getOutgoingEdges())
    {
        dependencies.push_back(edge->getTo());
    }

    return dependencies;
}

bool Dag::hasCycleDFS(Node* node, std::unordered_map<Node*, int>& colors, std::vector<Node*>& path,
                      std::vector<std::vector<Node*>>& cycles) const
{
    colors[node] = 1; // 标记为灰色（正在处理）
    path.push_back(node);

    // 获取逻辑依赖的节点列表
    std::vector<Node*> logicalDependencies = getLogicalDependencies(node);

    for (Node* neighbor : logicalDependencies)
    {
        if (colors[neighbor] == 1) // 发现后向边，即环路
        {
            // 构造环路路径
            std::vector<Node*> cycle;
            bool               found = false;
            for (auto it = path.rbegin(); it != path.rend(); ++it)
            {
                cycle.insert(cycle.begin(), *it);
                if (*it == neighbor)
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                cycles.push_back(cycle);
            }
            return true; // 找到环路就返回
        }
        else if (colors[neighbor] == 0) // 白色节点，继续DFS
        {
            if (hasCycleDFS(neighbor, colors, path, cycles))
            {
                return true; // 在子树中找到环路
            }
        }
    }

    path.pop_back();
    colors[node] = 2; // 标记为黑色（已完成）
    return false;
}
} // namespace Play::RDG
