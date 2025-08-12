#ifndef MODELLOADER_H
#define MODELLOADER_H
#include "nvh/fileoperations.hpp"
#include "Resource.h"
#include "nvvk/debug_util_vk.hpp"
#include <assimp/scene.h>
// #include "PlayApp.h"
namespace Play
{
class PlayApp;
struct SceneNode;
class ModelLoader
{
   public:
    void init(PlayApp* app);

    void                 loadModel(std::string path);
    std::vector<Buffer>& getSceneVBuffers()
    {
        return _sceneVBuffers;
    }
    std::vector<Buffer>& getSceneIBuffers()
    {
        return _sceneIBuffers;
    }
    std::vector<Mesh>& getSceneMeshes()
    {
        return _sceneMeshes;
    }
    std::vector<Material>& getSceneMaterials()
    {
        return _sceneMaterials;
    }

    Buffer* getMaterialBuffer()
    {
        return _materialBuffer;
    }
    Buffer* getInstanceBuffer()
    {
        return _instanceBuffer;
    }

    Buffer* getLightMeshIdxBuffer()
    {
        return _lightMeshIdxBuffer;
    }

    std::vector<Texture*>& getSceneTextures()
    {
        return _sceneTextures;
    }

    struct alignas(64) DynamicStruct {
        glm::mat4 model;
        uint32_t matIdx;
    };

    std::vector<DynamicStruct> getInstanceData()
    {
        return _dynamicUniformData;
    }

    Buffer* getDynamicUniformBuffer()
    {
        return _dynamicUniformBuffer;
    }
    
   private:
    PlayApp* _app;
    std::vector<Texture*> _sceneTextures;
    std::vector<Buffer>   _sceneVBuffers;
    std::vector<Buffer>   _sceneIBuffers;
    std::vector<Mesh>     _sceneMeshes;
    std::vector<Material> _sceneMaterials;
    std::vector<int32_t>  _emissiveMeshIdx;
    std::vector<DynamicStruct> _dynamicUniformData;
    nvvk::DebugUtil       m_debug;

    Buffer* _materialBuffer;
    Buffer* _instanceBuffer;
    Buffer* _lightMeshIdxBuffer;
    Buffer* _dynamicUniformBuffer;

    // Assimp processing functions
    void processAssimpNode(aiNode* node, const aiScene* scene, SceneNode* parentNode, uint32_t meshOffset, uint32_t materialOffset);
    uint32_t processAssimpMesh(aiMesh* mesh, const aiScene* scene, uint32_t meshOffset, uint32_t materialOffset);
    void loadAssimpTexture(const std::string& texturePath);

}; // namespace Play
} // namespace Play

#endif // MODELLOADER_H