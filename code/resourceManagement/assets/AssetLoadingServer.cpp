#include "AssetLoadingServer.h"
#include "core/JobSystem.h"
#include "core/Profiling.h"
#include "resourceManagement/assets/model/ModelUpload.h"

namespace Play
{

struct AssetLoadingServer::State
{
    std::vector<ModelLoadRequest>    requests;
    std::vector<uint32_t>            pendingRequests;
    std::vector<uint32_t>            pendingUploadRequests;
    std::vector<ModelLoadCompletion> completedModels;
    std::vector<ModelGpuUploadCompletion> completedModelUploads;
    std::vector<vpgloader::ModelHandle> loadedModels;
    uint32_t                         nextPendingRequest = 0;
    uint32_t                         nextPendingUploadRequest = 0;
    uint32_t                         nextCompletedModel = 0;
    uint32_t                         nextCompletedModelUpload = 0;
    uint32_t                         generation          = 1;
    std::mutex                       mutex;
};

AssetLoadingServer::AssetLoadingServer() : _state(std::make_shared<State>())
{
    _gpuUploader.initialize();
}

AssetLoadingServer::~AssetLoadingServer()
{
    _gpuUploader.deinitialize();
}

void AssetLoadingServer::clear()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::clear");

    std::lock_guard<std::mutex> lock(_state->mutex);
    _state->requests.clear();
    _state->pendingRequests.clear();
    _state->pendingUploadRequests.clear();
    _state->completedModels.clear();
    _state->completedModelUploads.clear();
    _state->loadedModels.clear();
    _state->nextPendingRequest = 0;
    _state->nextPendingUploadRequest = 0;
    _state->nextCompletedModel = 0;
    _state->nextCompletedModelUpload = 0;
    ++_state->generation;
    if (_state->generation == 0)
    {
        _state->generation = 1;
    }
}

ModelLoadRequestID AssetLoadingServer::requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                                        const vpgloader::ModelLoadOptions& options, AssetUploadPolicy uploadPolicy)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::requestModelLoad");

    std::lock_guard<std::mutex> lock(_state->mutex);

    ModelLoadRequest request;
    request.id            = makeRequestID(*_state, static_cast<uint32_t>(_state->requests.size()));
    request.requester     = requester;
    request.path          = path;
    request.options = options;
    request.uploadPolicy  = uploadPolicy;
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
        JobSystem::detach([state, request]()
        {
            ModelLoadCompletion completion;
            completion.request = request;
            try
            {
                PLAY_PROFILE_SCOPE("AssetLoadingServer::load model job");
                PLAY_PROFILE_MARK("Model load begin");
                completion.model = vpgloader::ModelLoader::Load(request.path, request.options);
                PLAY_PROFILE_MARK("Model load end");
            }
            catch (const std::exception& error)
            {
                completion.message = error.what();
            }

            const ModelLoadRequestState completedState =
                completion.model ? ModelLoadRequestState::eCpuLoaded : ModelLoadRequestState::eFailed;
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
            if (completion.model)
            {
                if (state->loadedModels.size() <= request.id.index)
                {
                    state->loadedModels.resize(request.id.index + 1);
                }
                state->loadedModels[request.id.index] = completion.model;
                if (storedRequest.uploadPolicy == AssetUploadPolicy::eUploadToGpu)
                {
                    state->pendingUploadRequests.push_back(request.id.index);
                }
            }
            state->completedModels.push_back(std::move(completion));
        });
    }
}

void AssetLoadingServer::processPendingUploads()
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::processPendingUploads");

    struct PendingUpload
    {
        ModelLoadRequest                 request;
        vpgloader::ModelHandle source;
    };

    std::vector<PendingUpload> uploadsToStart;
    {
        std::lock_guard<std::mutex> lock(_state->mutex);
        while (_state->nextPendingUploadRequest < _state->pendingUploadRequests.size())
        {
            const uint32_t requestIndex = _state->pendingUploadRequests[_state->nextPendingUploadRequest++];
            if (requestIndex >= _state->requests.size() || requestIndex >= _state->loadedModels.size())
            {
                continue;
            }

            ModelLoadRequest& request = _state->requests[requestIndex];
            if (request.state != ModelLoadRequestState::eCpuLoaded || request.uploadPolicy != AssetUploadPolicy::eUploadToGpu)
            {
                continue;
            }

            const vpgloader::ModelHandle source = _state->loadedModels[requestIndex];
            if (!source)
            {
                request.state = ModelLoadRequestState::eFailed;

                ModelGpuUploadCompletion completion;
                completion.request = request;
                completion.message = "Model GPU upload has no CPU source data.";
                _state->completedModelUploads.push_back(std::move(completion));
                continue;
            }

            request.state = ModelLoadRequestState::eUploading;
            uploadsToStart.push_back({request, source});
        }

        if (_state->nextPendingUploadRequest >= _state->pendingUploadRequests.size())
        {
            _state->pendingUploadRequests.clear();
            _state->nextPendingUploadRequest = 0;
        }
    }

    for (const PendingUpload& pendingUpload : uploadsToStart)
    {
        const std::shared_ptr<ModelUploadJob> job = std::make_shared<ModelUploadJob>(pendingUpload.request.id, pendingUpload.source);
        if (_gpuUploader.enqueue(job).isValid())
        {
            continue;
        }

        std::lock_guard<std::mutex> lock(_state->mutex);
        if (pendingUpload.request.id.generation != _state->generation || pendingUpload.request.id.index >= _state->requests.size())
        {
            continue;
        }

        ModelLoadRequest& request = _state->requests[pendingUpload.request.id.index];
        if (request.id.generation != pendingUpload.request.id.generation || request.state != ModelLoadRequestState::eUploading)
        {
            continue;
        }

        request.state = ModelLoadRequestState::eFailed;
        ModelGpuUploadCompletion completion;
        completion.request = request;
        completion.message = "Asset GPU uploader is not available.";
        _state->completedModelUploads.push_back(std::move(completion));
    }

    AssetUploadCompletion uploadCompletion;
    while (_gpuUploader.popCompleted(uploadCompletion))
    {
        const std::shared_ptr<ModelUploadJob> job = std::dynamic_pointer_cast<ModelUploadJob>(uploadCompletion.job);
        if (!job)
        {
            continue;
        }

        ModelGpuUploadCompletion completion;
        completion.success = uploadCompletion.success;
        completion.message = uploadCompletion.message;
        if (completion.success)
        {
            completion.model = job->takeUploadedModel();
        }

        const ModelLoadRequestID requestID = job->getRequestID();
        std::lock_guard<std::mutex> lock(_state->mutex);
        if (requestID.generation != _state->generation || requestID.index >= _state->requests.size())
        {
            continue;
        }

        ModelLoadRequest& request = _state->requests[requestID.index];
        if (request.id.generation != requestID.generation || request.state != ModelLoadRequestState::eUploading)
        {
            continue;
        }

        request.state       = completion.success ? ModelLoadRequestState::eGpuUploaded : ModelLoadRequestState::eFailed;
        completion.request  = request;
        if (!completion.success && completion.message.empty())
        {
            completion.message = "Model GPU upload failed.";
        }
        _state->completedModelUploads.push_back(std::move(completion));
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

bool AssetLoadingServer::popCompletedModelUpload(ModelGpuUploadCompletion& completion)
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::popCompletedModelUpload");

    std::lock_guard<std::mutex> lock(_state->mutex);
    if (_state->nextCompletedModelUpload >= _state->completedModelUploads.size())
    {
        _state->completedModelUploads.clear();
        _state->nextCompletedModelUpload = 0;
        return false;
    }

    completion = std::move(_state->completedModelUploads[_state->nextCompletedModelUpload++]);
    return true;
}

vpgloader::ModelHandle AssetLoadingServer::getLoadedModel(ModelLoadRequestID id) const
{
    PLAY_PROFILE_SCOPE("AssetLoadingServer::getLoadedModel");

    std::lock_guard<std::mutex> lock(_state->mutex);
    if (!id.isValid() || id.generation != _state->generation || id.index >= _state->loadedModels.size())
    {
        return nullptr;
    }

    return _state->loadedModels[id.index];
}

ModelLoadRequestID AssetLoadingServer::makeRequestID(const State& state, uint32_t index)
{
    ModelLoadRequestID id;
    id.index      = index;
    id.generation = state.generation;
    return id;
}

} // namespace Play
