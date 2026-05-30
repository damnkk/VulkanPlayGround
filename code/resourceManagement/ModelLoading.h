#ifndef MODEL_LOADING_H
#define MODEL_LOADING_H

#include "ModelLoadingConfig.h"
#include "SceneAssets.h"

namespace Play
{

struct ImportedModel
{
    ModelAssetPackage package;
};

struct OptimizedModel
{
    ModelAssetPackage package;
};

struct ModelImportResult
{
    bool          success = false;
    ImportedModel model;
    std::string   message;
};

struct ModelOptimizeResult
{
    bool           success = false;
    OptimizedModel model;
    std::string    message;
};

struct ModelLoadResult
{
    bool              success = false;
    ModelAssetPackage model;
    std::string       message;
};

namespace model_loading
{

ModelImportResult   importModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig);
ModelOptimizeResult optimizeModel(ImportedModel&& importedModel, const ModelLoadingConfig& loadingConfig);
ModelLoadResult     loadModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig);

} // namespace model_loading

} // namespace Play

#endif // MODEL_LOADING_H
