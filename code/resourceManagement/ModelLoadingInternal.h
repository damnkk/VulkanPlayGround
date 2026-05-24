#ifndef MODEL_LOADING_INTERNAL_H
#define MODEL_LOADING_INTERNAL_H

#include "ModelLoading.h"

namespace Play::model_loading
{

ModelLoadResult loadModelAssetFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg, AssetRegistry& assets);

} // namespace Play::model_loading

#endif // MODEL_LOADING_INTERNAL_H
