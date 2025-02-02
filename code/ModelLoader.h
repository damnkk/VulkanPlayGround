#ifndef MODELLOADER_H
#define MODELLOADER_H
#include "nvh/fileoperations.hpp"
#include "Resource.h"
#include "nvvk/debug_util_vk.hpp"
// #include "PlayApp.h"
namespace Play
{
class PlayApp;
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

    Buffer& getMaterialBuffer()
    {
        return _materialBuffer;
    }
    Buffer& getInstanceBuffer()
    {
        return _instanceBuffer;
    }

    Buffer& getLightMeshIdxBuffer()
    {
        return _lightMeshIdxBuffer;
    }

    std::vector<Texture>& getSceneTextures()
    {
        return _sceneTextures;
    }

   private:
    PlayApp* _app;
    std::vector<Texture>  _sceneTextures;
    std::vector<Buffer>   _sceneVBuffers;
    std::vector<Buffer>   _sceneIBuffers;
    std::vector<Mesh>     _sceneMeshes;
    std::vector<Material> _sceneMaterials;
    std::vector<int32_t>  _emissiveMeshIdx;
    nvvk::DebugUtil       m_debug;

    Buffer _materialBuffer;
    Buffer _instanceBuffer;
    Buffer _lightMeshIdxBuffer;

}; // namespace Play
} // namespace Play

#endif // MODELLOADER_H