#include "SceneNode.h"

namespace Play
{

std::shared_ptr<SceneNode> SceneNode::addChild(nvh::GltfNode child)
{
    std::shared_ptr<SceneNode> node = std::make_shared<SceneNode>();
    node->_transform                = child.worldMatrix;
    node->_name                     = child.tnode->name;
    node->_parent                   = this;
    this->_children.push_back(node);
    return node;
}

std::shared_ptr<SceneNode> SceneNode::addChild(std::string name)
{
    std::shared_ptr<SceneNode> node = std::make_shared<SceneNode>();
    node->_name                     = name;
    node->_parent                   = this;
    this->_children.push_back(node);
    return node;
}
} // namespace Play