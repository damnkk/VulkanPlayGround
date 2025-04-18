#ifndef VOLUMERENDERER_H
#define VOLUMERENDERER_H
#include "Renderer.h"
#include "glm/glm.hpp"
#include "nvvk/pipeline_vk.hpp"
#include <vulkan/vulkan_core.h>
namespace Play
{
template <uint32_t N>
struct PiecewiseFunction
{
    float                rangeMin = -1024.0f;
    float                rangeMax = 3071.0f;
    uint32_t             Count    = 0;
    std::array<float, N> Position = {};
    std::array<float, N> Value    = {};
};

// we have a json file to map different CT intensity to different render params
// this will help us split out bones and veins or muscles, we generate a lookup table
// here to pass the map to shader
template <uint32_t N = 64>
class PiecewiseLinearFunction : public PiecewiseFunction<N>
{
   public:
    void addNode(float position, float value)
    {
        this->Position[this->Count] = position;
        this->Value[this->Count]    = value;
        ++this->Count;
    }
    float evaluate(float positionNormalized) const
    {
        auto position = positionNormalized * (this->rangeMax - this->rangeMin) + this->rangeMin;
        if (this->Count <= 0)
        {
            return 0.0f;
        }
        if (position < this->rangeMin)
        {
            return this->Value[0];
        }
        if (position > this->rangeMax)
        {
            return this->Value[this->Count - 1];
        }
        for (int i = 1; i < this->Count; ++i)
        {
            auto const p1 = this->Position[i - 1];
            auto const p2 = this->Position[i];
            auto const t  = (position - p1) / (p2 - p1);
            if (position >= p1 && position < p2)
                return this->Value[i - 1] + t * (this->Value[i] - this->Value[i - 1]);
        }
        return 0;
    }
    void clear()
    {
        this->Count = 0;
    }
};

class ScalarTransferFunction1D
{
   public:
    void addNode(float position, float value)
    {
        this->PLF.addNode(position, value);
    }
    float evaluate(float intensity) const
    {
        return this->PLF.evaluate(intensity);
    }
    std::vector<uint8_t> getLookUpData(uint32_t sampleNum = 64)
    {
        std::vector<uint8_t> data(sampleNum);
        for (int i = 0; i < sampleNum; ++i)
        {
            data[i] = static_cast<uint8_t>(
                std::round(255.0f * this->evaluate(float(i) / static_cast<float>(sampleNum - 1))));
        }
        return data;
    }
    void clear()
    {
        PLF.clear();
    }
    PiecewiseLinearFunction<> PLF;
};

class ColorTransferFunction1D
{
   public:
    void addNode(float position, glm::vec3 value)
    {
        this->PLF0.addNode(position, value.x);
        this->PLF1.addNode(position, value.y);
        this->PLF2.addNode(position, value.z);
    }

    glm::vec3 Evaluate(float intensity) const
    {
        return glm::vec3{this->PLF0.evaluate(intensity), this->PLF1.evaluate(intensity),
                         this->PLF2.evaluate(intensity)};
    }

    std::vector<glm::vec4> getLookUpData(uint32_t sampleNum)
    {
        std::vector<glm::vec4> data(sampleNum);
        for (size_t index = 0; index < sampleNum; index++)
        {
            glm::vec3  v = this->Evaluate(index / static_cast<float>(sampleNum - 1));
            const auto x = static_cast<uint8_t>(std::round(255.0f * v.x));
            const auto y = static_cast<uint8_t>(std::round(255.0f * v.y));
            const auto z = static_cast<uint8_t>(std::round(255.0f * v.z));
            data[index]  = glm::vec4(x, y, z, static_cast<uint8_t>(0));
        }
        return data;
    }

    void Clear()
    {
        this->PLF0.clear();
        this->PLF1.clear();
        this->PLF2.clear();
    }

    PiecewiseLinearFunction<> PLF0;
    PiecewiseLinearFunction<> PLF1;
    PiecewiseLinearFunction<> PLF2;
};

struct Scene;
struct PlayApp;
struct Texture;
struct VolumeTexture;
class VolumeRenderer : public Renderer
{
    public:
    VolumeRenderer(PlayApp& app);
    ~VolumeRenderer() override = default;
    void OnPreRender() override;
    void OnPostRender() override;
    void RenderFrame() override;
    void SetScene(Scene* scene) override;
    void OnResize(int width, int height) override;
    void OnDestroy() override;
    Texture* getOutputTexture() override
    {
        return _postProcessRT;
    };

   protected:
    void loadVolumeTexture(const std::string& filename);
    void createRenderResource();
    private:
    struct VolumeRenderConstanBuffer
    {
        glm::mat4 viewMatrix = glm::mat4(1.0);
        glm::mat4 inverseViewMatrix = glm::mat4(1.0);
        glm::mat4 projMatrix = glm::mat4(1.0);
        glm::mat4 inverseProjMatrix = glm::mat4(1.0);
        glm::vec3 cameraPos = glm::vec3(0.0);
        uint32_t  FrameCount=0;
    } _vConstant;

    VolumeTexture* _volumeTexture          = nullptr;
    Texture* _gradientTexture    = nullptr;
    Texture* _diffuseLookUpTexture = nullptr;
    Texture* _specularLookUpTexture = nullptr;
    Texture* _roughnessLookUpTexture = nullptr;
    Texture* _opacityLookUpTexture   = nullptr;

    Texture* _diffuseRT = nullptr;
    Texture* _specularRT = nullptr;
    Texture* _radianceRT = nullptr;
    Texture* _normalRT   = nullptr;
    struct DepthTexture
    {
        VkImage        image;
        VkDeviceMemory memory;
        VkImageLayout  layout;
        VkImageView    view;
    } _depthRT;

    Texture* _accumulateRT = nullptr;
    Texture* _postProcessRT = nullptr;
    PlayApp* _app;

    VkPipeline _generateRaysPipeline;
    VkPipeline _radiancesPipeline;
    VkPipeline _accumulatePipeline;
    VkPipeline _postProcessPipeline;

    ColorTransferFunction1D  _diffuseTransferFunc;
    ColorTransferFunction1D  _specularTransferFunc;
    ScalarTransferFunction1D _roughnessTransferFunc;
    ScalarTransferFunction1D _opacityTransferFunc;
};

} // namespace Play
#endif // VOLUMERENDERER_H