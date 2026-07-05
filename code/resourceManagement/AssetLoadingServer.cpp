#include "AssetLoadingServer.h"
#include "core/JobSystem.h"
#include "core/Profiling.h"

namespace Play
{

struct AssetLoadingServer::State
{
    std::vector<ModelLoadRequest>    requests;
    std::vector<uint32_t>            pendingRequests;
    std::vector<ModelLoadCompletion> completedModels;
    uint32_t                         nextPendingRequest = 0;
    uint32_t                         nextCompletedModel = 0;
    uint32_t                         generation          = 1;
    std::mutex                       mutex;
};

AssetLoadingServer::AssetLoadingServer() : _state(std::make_shared<State>()) {}

void AssetLoadingServer::clear()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::clear");

    std::lock_guard<std::mutex> lock(_state->mutex);
    _state->requests.clear();
    _state->pendingRequests.clear();
    _state->completedModels.clear();
    _state->nextPendingRequest = 0;
    _state->nextCompletedModel = 0;
    ++_state->generation;
    if (_state->generation == 0)
    {
        _state->generation = 1;
    }
}

ModelLoadRequestID AssetLoadingServer::requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                                        const ModelLoadingConfig& loadingConfig)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::requestModelLoad");

    std::lock_guard<std::mutex> lock(_state->mutex);

    ModelLoadRequest request;
    request.id            = makeRequestID(*_state, static_cast<uint32_t>(_state->requests.size()));
    request.requester     = requester;
    request.path          = path;
    request.loadingConfig = loadingConfig;
    request.state         = ModelLoadRequestState::eQueued;

    _state->requests.push_back(request);
    _state->pendingRequests.push_back(request.id.index);
    return request.id;
}

void AssetLoadingServer::processPendingLoads()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::processPendingLoads");

    std::vector<ModelLoadRequest> requestsToStart;
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        while (_state->nextPendingRequest < _state->pendingRequests.size())
        {
            const uint32_t requestIndex = _state->pendingRequests[_state->nextPendingRequest++];
            if (requestIndex >= _state->requests.size())
            {
                continue;
            }

            ModelLoadRequest& request = _state->requests[requestIndex];
            if (request.state != ModelLoadRequestState::eQueued)
            {
                continue;
            }

            request.state = ModelLoadRequestState::eLoading;
            requestsToStart.push_back(request);
        }

        if (_state->nextPendingRequest >= _state->pendingRequests.size())
        {
            _state->pendingRequests.clear();
            _state->nextPendingRequest = 0;
        }
    }

    for (ModelLoadRequest request : requestsToStart)
    {
        std::shared_ptr<State> state = _state;
        JobSystem::detach(
            [state, request]()
            {
                ModelLoadCompletion completion;
                completion.request = request;
                {
                    PLAY_PROFILE_SCOPE("AssetLoadingServer::load model job");
                    PLAY_PROFILE_MARK("Model load begin");
                    completion.result = model_loading::loadModelFromFile(request.path, request.loadingConfig);
                    PLAY_PROFILE_MARK("Model load end");
                }

                const ModelLoadRequestState completedState =
                    completion.result.success ? ModelLoadRequestState::eCompleted : ModelLoadRequestState::eFailed;
                completion.request.state = completedState;

                std::lock_guard<std::mutex> lock(state->mutex);
                if (request.id.generation != state->generation || request.id.index >= state->requests.size())
                {
                    return;
                }

                ModelLoadRequest& storedRequest = state->requests[request.id.index];
                if (storedRequest.id.generation != request.id.generation || storedRequest.state != ModelLoadRequestState::eLoading)
                {
                    return;
                }

                storedRequest.state = completedState;
                state->completedModels.push_back(std::move(completion));
            });
    }
}

bool AssetLoadingServer::popCompletedModel(ModelLoadCompletion& completion)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::popCompletedModel");

    std::lock_guard<std::mutex> lock(_state->mutex);
    if (_state->nextCompletedModel >= _state->completedModels.size())
    {
        _state->completedModels.clear();
        _state->nextCompletedModel = 0;
        return false;
    }

    completion = std::move(_state->completedModels[_state->nextCompletedModel++]);
    return true;
}

ModelLoadRequestID AssetLoadingServer::makeRequestID(const State& state, uint32_t index)
{
    ModelLoadRequestID id;
    id.index      = index;
    id.generation = state.generation;
    return id;
}

} // namespace Play
