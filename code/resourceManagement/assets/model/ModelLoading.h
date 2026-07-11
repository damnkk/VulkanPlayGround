#ifndef MODEL_LOADING_H
#define MODEL_LOADING_H

#include "ModelLoadingConfig.h"
#include "ModelAssets.h"

namespace Play
{

struct ModelLoadResult
{
    bool              success = false;
    ModelAssetPackage model;
    std::string       message;
};

namespace model_loading
{

ModelLoadResult loadModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig);

} // namespace model_loading

} // namespace Play

#endif // MODEL_LOADING_H
