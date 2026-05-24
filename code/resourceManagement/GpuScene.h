#ifndef GPU_SCENE_H
#define GPU_SCENE_H

#include "SceneAssets.h"

namespace Play
{

enum class GpuSceneType : uint32_t
{
    eRaster,
    eGaussian,
    eRayTracing
};

class GpuScene
{
public:
    virtual ~GpuScene() = default;

    virtual GpuSceneType getType() const = 0;
    virtual void         clear()         = 0;
    virtual void         rebuild(const CpuScene& scene, AssetRegistry& assets) = 0;

    virtual uint64_t getSourceSceneRevision() const
    {
        return 0;
    }
};

class RayTracingGpuScene : public GpuScene
{
public:
    GpuSceneType getType() const override
    {
        return GpuSceneType::eRayTracing;
    }

    void clear() override {}
    void rebuild(const CpuScene& scene, AssetRegistry& assets) override
    {
        (void) scene;
        (void) assets;
    }
};

} // namespace Play

#endif // GPU_SCENE_H
