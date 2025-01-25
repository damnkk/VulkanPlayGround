#include "ModelLoader.h"
#include "nvh/nvprint.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
namespace Play
{

void ModelLoader::loadModel(std::string path)
{
    Assimp::Importer importer;
    unsigned int     flags = 0 | aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals |
                         aiProcess_CalcTangentSpace;
    const aiScene* res = importer.ReadFile(path, flags);

    if (!res)
    {
        LOGE_FILELINE("Failed to load model: %s\n", path.c_str());
    }
    else
    {
        LOGI("Model loaded successfully: %s\n", path.c_str());
    }
    aiNode* rootNode = res->mRootNode;
}

} // namespace Play