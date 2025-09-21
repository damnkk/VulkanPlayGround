#include "Resource.h"
#include <nvvk/descriptors.hpp>
#include <functional>
#include "ShaderManager.hpp"

namespace Play
{

class PlayElement;
using PipelineKey = std::size_t;

class GraphicsPipelineStateWithKey : public GraphicsPipelineState
{
public:
    PipelineKey getPipelineKey() const;

private:
    PipelineKey _pipelineKey = ~0U;
};

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
    PipelineCacheManager(PlayElement* element);
    virtual ~PipelineCacheManager() {};
    void getOrCreateComputePipeline(const ComputePipelineStateWithKey&   cState,
                                    std::function<void(VkPipelineCache)> createCPipelineFunc);
    void getOrCreateGraphicsPipeline(const GraphicsPipelineStateWithKey&  gState,
                                     std::function<void(VkPipelineCache)> createGfxPipelineFunc);

private:
};

} // namespace Play