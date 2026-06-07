#ifndef GBUFFERPASS_H
#define GBUFFERPASS_H
#include "RenderPass.h"
#include "GBufferConfig.h"
#include "SceneAssets.h"
#include "Resource.h"
#include "Hdevice.h"
#include <rttr/rttr_enable.h>
namespace Play
{
class DeferRenderer;
class GpuScene;
class CpuScene;

struct GBufferVisibleInstance
{
    uint32_t  modelIndex       = INVALID_SCENE_ID;
    uint32_t  firstRenderable  = 0;
    uint32_t  renderableCount  = 0;
    glm::mat4 objectToWorld    = glm::mat4(1.0f);
    AABB      worldBounds;
    float     depthKey         = 0.0f;
};

struct GBufferRenderItem
{
    uint64_t sortKey              = 0;
    float    depthKey             = 0.0f;
    uint32_t visibleInstanceIndex = INVALID_SCENE_ID;
    uint32_t renderableIndex      = INVALID_SCENE_ID;
    uint32_t meshInfoIndex        = INVALID_SCENE_ID;
    uint32_t materialIndex        = INVALID_SCENE_ID;
    uint32_t indexCount           = 0;
    uint32_t gpuInstanceIndex     = INVALID_SCENE_ID;
};

struct GBufferGPUInstanceData
{
    glm::mat4 objectToWorld      = glm::mat4(1.0f);
    glm::mat4 worldToObject      = glm::mat4(1.0f);
    uint64_t  meshInfoAddress    = 0;
    uint64_t  materialAddress    = 0;
    uint64_t  textureInfoAddress = 0;
    uint32_t  meshInfoIndex      = INVALID_SCENE_ID;
    uint32_t  materialIndex      = INVALID_SCENE_ID;
    uint32_t  textureInfoOffset  = 0;
    uint32_t  flags              = 0;
};

class GBufferPass : public BasePass
{
public:
    GBufferPass() = default;
    GBufferPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~GBufferPass() = default;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;

    RTTR_ENABLE(BasePass)

private:
    void prepareRenderList();
    void collectVisibleInstances(const CpuScene& scene, const GpuScene& gpuScene, const CameraData& cameraData);
    void buildRenderList(const GpuScene& gpuScene);
    void sortRenderList();
    void uploadGPUInstanceData();

    DeferRenderer* _ownedRender = nullptr;
    std::vector<GBufferVisibleInstance> _visibleInstances;
    std::vector<GBufferRenderItem>      _renderItems;
    std::vector<GBufferGPUInstanceData> _gpuInstanceData;
    RefPtr<Buffer>                      _gpuInstanceDataBuffer = nullptr;
};
} // namespace Play

#endif // GBUFFERPASS_H
