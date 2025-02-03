#ifndef SCNENE_NODE_H
#define SCNENE_NODE_H
#include "glm/glm.hpp"
#include "vector"
#include "string"
#include "memory"
#include "nvh/gltfscene.hpp"
namespace Play
{
struct SceneNode
{
    std::shared_ptr<SceneNode>                                    addChild(nvh::GltfNode child);
    std::shared_ptr<SceneNode>                                    addChild(std::string name);
    glm::mat4               _transform = glm::mat4(1.0);
    SceneNode*              _parent    = nullptr;
    std::vector<std::shared_ptr<SceneNode>>                       _children;
    std::string             _name;
    std::vector<uint32_t>                                         _meshIdx;
};

struct Scene
{
    Scene() : _root(std::make_shared<SceneNode>()){};
    std::shared_ptr<SceneNode> _root;
};
} // namespace Play

#endif // SCNENE_NODE_H