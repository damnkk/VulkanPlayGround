#include "SceneNode.h"

namespace Play
{
glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& aiMat)
{
    glm::mat4 glmMat;
    glmMat[0][0] = aiMat.a1;
    glmMat[0][1] = aiMat.b1;
    glmMat[0][2] = aiMat.c1;
    glmMat[0][3] = aiMat.d1;
    glmMat[1][0] = aiMat.a2;
    glmMat[1][1] = aiMat.b2;
    glmMat[1][2] = aiMat.c2;
    glmMat[1][3] = aiMat.d2;
    glmMat[2][0] = aiMat.a3;
    glmMat[2][1] = aiMat.b3;
    glmMat[2][2] = aiMat.c3;
    glmMat[2][3] = aiMat.d3;
    glmMat[3][0] = aiMat.a4;
    glmMat[3][1] = aiMat.b4;
    glmMat[3][2] = aiMat.c4;
    glmMat[3][3] = aiMat.d4;

    return glmMat;
}

std::shared_ptr<SceneNode> SceneNode::addChild(aiNode* child)
{
    if (child == nullptr)
    {
        return nullptr;
    }
    std::shared_ptr<SceneNode> node = std::make_shared<SceneNode>();
    node->_transform                = this->_transform * aiMatrix4x4ToGlm(child->mTransformation);
    node->_name                     = std::string(child->mName.C_Str());
    node->_parent                   = this;
    this->_children.push_back(node);
    return node;
}

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