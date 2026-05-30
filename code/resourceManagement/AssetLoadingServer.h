#ifndef ASSET_LOADING_SERVER_H
#define ASSET_LOADING_SERVER_H

#include "ModelLoading.h"

namespace Play
{

enum class ModelLoadRequestState : uint32_t
{
    eQueued,
    eLoading,
    eCompleted,
    eFailed
};

struct ModelLoadRequest
{
    ModelLoadRequestID     id;
    CpuSceneComponentID    requester;
    std::filesystem::path  path;
    ModelLoadingConfig     loadingConfig;
    ModelLoadRequestState  state = ModelLoadRequestState::eQueued;
};

struct ModelLoadCompletion
{
    ModelLoadRequest request;
    ModelLoadResult  result;
};

class AssetLoadingServer
{
public:
    void clear();

    ModelLoadRequestID requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                        const ModelLoadingConfig& loadingConfig);

    void processPendingLoads();
    bool popCompletedModel(ModelLoadCompletion& completion);

private:
    ModelLoadRequestID makeRequestID(uint32_t index) const;

    std::vector<ModelLoadRequest>    _requests;
    std::vector<uint32_t>            _pendingRequests;
    std::vector<ModelLoadCompletion> _completedModels;
    uint32_t                         _nextPendingRequest = 0;
    uint32_t                         _nextCompletedModel = 0;
};

} // namespace Play

#endif // ASSET_LOADING_SERVER_H
