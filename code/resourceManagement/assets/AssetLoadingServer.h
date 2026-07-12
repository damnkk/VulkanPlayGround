#ifndef ASSET_LOADING_SERVER_H
#define ASSET_LOADING_SERVER_H

#include "resourceManagement/assets/model/ModelLoading.h"
#include "resourceManagement/assets/model/ModelGpuAssets.h"
#include "resourceManagement/assets/upload/AssetGpuUploader.h"

namespace Play
{

enum class ModelLoadRequestState : uint32_t
{
    eQueued,
    eLoading,
    eCpuLoaded,
    eUploading,
    eGpuUploaded,
    eFailed
};

// Loading CPU data and creating GPU resources are intentionally independent. A caller
// can retain a LoadedModel for tooling or preprocessing without creating Vulkan objects.
enum class AssetUploadPolicy : uint32_t
{
    eCpuOnly,
    eUploadToGpu
};

struct ModelLoadRequest
{
    ModelLoadRequestID     id;
    CpuSceneComponentID    requester;
    std::filesystem::path  path;
    ModelLoadingConfig     loadingConfig;
    AssetUploadPolicy      uploadPolicy = AssetUploadPolicy::eUploadToGpu;
    ModelLoadRequestState  state = ModelLoadRequestState::eQueued;
};

struct ModelLoadCompletion
{
    ModelLoadRequest request;
    ModelLoadResult  result;
};

struct ModelGpuUploadCompletion
{
    ModelLoadRequest request;
    UploadedModel    model;
    bool             success = false;
    std::string      message;
};

class AssetLoadingServer
{
public:
    AssetLoadingServer();
    ~AssetLoadingServer();

    void clear();

    ModelLoadRequestID requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                        const ModelLoadingConfig& loadingConfig,
                                        AssetUploadPolicy uploadPolicy = AssetUploadPolicy::eUploadToGpu);

    void processPendingLoads();
    void processPendingUploads();
    bool popCompletedModel(ModelLoadCompletion& completion);
    bool popCompletedModelUpload(ModelGpuUploadCompletion& completion);
    std::shared_ptr<const LoadedModel> getLoadedModel(ModelLoadRequestID id) const;

    AssetGpuUploader& getGpuUploader()
    {
        return _gpuUploader;
    }

private:
    struct State;

    static ModelLoadRequestID makeRequestID(const State& state, uint32_t index);

    AssetGpuUploader       _gpuUploader;
    std::shared_ptr<State> _state;
};

} // namespace Play

#endif // ASSET_LOADING_SERVER_H
