#ifndef ASSET_LOADING_SERVER_H
#define ASSET_LOADING_SERVER_H

#include "resourceManagement/assets/model/ModelLoading.h"

namespace Play
{

enum class ModelLoadRequestState : uint32_t
{
    eQueued,
    eLoading,
    eCpuLoaded,
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
    AssetLoadingServer();

    void clear();

    ModelLoadRequestID requestModelLoad(CpuSceneComponentID requester, const std::filesystem::path& path,
                                        const ModelLoadingConfig& loadingConfig);

    void processPendingLoads();
    bool popCompletedModel(ModelLoadCompletion& completion);
    std::shared_ptr<const LoadedModel> getLoadedModel(ModelLoadRequestID id) const;

private:
    struct State;

    static ModelLoadRequestID makeRequestID(const State& state, uint32_t index);

    std::shared_ptr<State> _state;
};

} // namespace Play

#endif // ASSET_LOADING_SERVER_H
