#ifndef PLAY_SCENE_H
#define PLAY_SCENE_H

#include "GpuScene.h"
#include "core/RefCounted.h"
#include "miniply.h"
#include "newShaders/gaussian/gaussianLib.h.slang"
#include <filesystem>
#include <splat-types.h>
#include <vector>

namespace Play
{

class Buffer;

class GaussianScene : public GpuScene
{
public:
    GpuSceneType getType() const override
    {
        return GpuSceneType::eGaussian;
    }

    void clear() override;
    void rebuild(const CpuScene& scene, AssetRegistry& assets) override
    {
        (void) scene;
        (void) assets;
    }

    bool load(const std::filesystem::path& path);

    const std::vector<float3>& getPositions() const
    {
        return _positions;
    }

    const std::vector<float4>& getColors() const
    {
        return _colors;
    }

    const std::vector<float>& getCovariances() const
    {
        return _covariances;
    }

    const std::vector<float>& getRotations() const
    {
        return _rotations;
    }

    const std::vector<float>& getShRestCoefficients() const
    {
        return _shRestCoefficients;
    }

    const GaussianSceneMeta& getMeta() const
    {
        return _meta;
    }

    uint32_t getVertexCount() const
    {
        return static_cast<uint32_t>(_positions.size());
    }

    Buffer* getSplatMetaGPUBuffer()
    {
        return _splatMetaBuffer.get();
    }

    Buffer* getPositionGPUBuffer()
    {
        return _positionBuffer.get();
    }

    Buffer* getColorGPUBuffer()
    {
        return _colorBuffer.get();
    }

    Buffer* getCovarianceGPUBuffer()
    {
        return _covarianceBuffer.get();
    }

    Buffer* getShRestGPUBuffer()
    {
        return _shRestBuffer.get();
    }

    Buffer* getSceneUniformBuffer()
    {
        return _sceneUniformBuffer.get();
    }

    void convertCoordinates(spz::CoordinateSystem from, spz::CoordinateSystem to);

private:
    std::vector<float3> _positions;
    std::vector<float4> _colors;
    std::vector<float>  _covariances;
    std::vector<float>  _rotations;
    std::vector<float>  _shRestCoefficients;

    RefPtr<Buffer> _positionBuffer;
    RefPtr<Buffer> _colorBuffer;
    RefPtr<Buffer> _covarianceBuffer;
    RefPtr<Buffer> _shRestBuffer;
    RefPtr<Buffer> _splatMetaBuffer;

    RefPtr<Buffer>    _sceneUniformBuffer;
    GaussianSceneMeta _meta{};
};

} // namespace Play

#endif // PLAY_SCENE_H
