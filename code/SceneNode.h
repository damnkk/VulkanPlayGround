#ifndef SCNENE_NODE_H
#define SCNENE_NODE_H
#include "glm/glm.hpp"
#include "vector"
#include "string"
struct SceneNode
{
    glm::mat4               _transform = glm::mat4(1.0);
    SceneNode*              _parent    = nullptr;
    std::vector<SceneNode*> _children;
    std::string             _name;
    int32_t                 _meshIdx = -1;
};

struct Scene
{
    SceneNode* _root;

    // std::vector<Material>
};

#endif // SCNENE_NODE_H