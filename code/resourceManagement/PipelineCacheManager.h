#include "Resource.h"
#include <nvvk/descriptors.hpp>
#include <nvvk/graphics_pipeline.hpp>
#include <functional>
#include "ShaderManager.hpp"

namespace Play
{

class PlayElement;
using PipelineKey = std::size_t;

class ComputePipelineStateWithKey
{
public:
    PipelineKey getPipelineKey() const;
    void        addShader(ShaderModule* shaderModule)
    {
        _shaderModule = shaderModule;
    }

private:
    ShaderModule* _shaderModule;
    PipelineKey   _pipelineKey = ~0U;
};

class PipelineCacheManager
{
public:
    PipelineCacheManager();
    virtual ~PipelineCacheManager() {};
    static PipelineCacheManager& Instance();
    void                         init();
    void                         deinit();
    void getOrCreateComputePipeline(const ComputePipelineStateWithKey& cState, std::function<void(VkPipelineCache)> createCPipelineFunc);

private:
    std::vector<VkPipeline> _pipelines;
};

} // namespace Play