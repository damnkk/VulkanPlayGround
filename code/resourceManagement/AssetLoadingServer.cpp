#include "AssetLoadingServer.h"
#include "core/Profiling.h"

namespace Play
{

void AssetLoadingServer::clear()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::clear");
    _requests.clear();
    _pendingRequests.clear();
    _completedModels.clear();
    _nextPendingRequest = 0;
    _nextCompletedModel = 0;
}

ModelLoadRequestID AssetLoadingServer::requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                                        const ModelLoadingConfig& loadingConfig)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::requestModelLoad");

    ModelLoadRequest request;
    request.id            = makeRequestID(static_cast<uint32_t>(_requests.size()));
    request.requester     = requester;
    request.path          = path;
    request.loadingConfig = loadingConfig;
    request.state         = ModelLoadRequestState::eQueued;

    _requests.push_back(request);
    _pendingRequests.push_back(request.id.index);
    return request.id;
}

void AssetLoadingServer::processPendingLoads()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::processPendingLoads");

    while (_nextPendingRequest < _pendingRequests.size())
    {
        const uint32_t requestIndex = _pendingRequests[_nextPendingRequest++];
        if (requestIndex >= _requests.size())
        {
            continue;
        }

        ModelLoadRequest& request = _requests[requestIndex];
        request.state             = ModelLoadRequestState::eLoading;

        ModelLoadCompletion completion;
        completion.request = request;
        {
            PLAY_PROFILE_SCOPE("AssetLoadingServer::load model");
            PLAY_PROFILE_MARK("Model load begin");
            completion.result  = model_loading::loadModelFromFile(request.path, request.loadingConfig);
            PLAY_PROFILE_MARK("Model load end");
        }

        request.state = completion.result.success ? ModelLoadRequestState::eCompleted : ModelLoadRequestState::eFailed;
        completion.request.state = request.state;
        _completedModels.push_back(std::move(completion));
    }

    if (_nextPendingRequest >= _pendingRequests.size())
    {
        _pendingRequests.clear();
        _nextPendingRequest = 0;
    }
}

bool AssetLoadingServer::popCompletedModel(ModelLoadCompletion& completion)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::popCompletedModel");

    if (_nextCompletedModel >= _completedModels.size())
    {
        _completedModels.clear();
        _nextCompletedModel = 0;
        return false;
    }

    completion = std::move(_completedModels[_nextCompletedModel++]);
    return true;
}

ModelLoadRequestID AssetLoadingServer::makeRequestID(uint32_t index) const
{
    ModelLoadRequestID id;
    id.index      = index;
    id.generation = index < _requests.size() ? _requests[index].id.generation : 1;
    return id;
}

} // namespace Play
