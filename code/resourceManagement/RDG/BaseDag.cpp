#include "BaseDag.h"

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
} // namespace Play::RDG
