#ifndef VOLUMERENDERER_H
#define VOLUMERENDERER_H
#include "Renderer.h"
#include "glm/glm.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/compute_vk.hpp"
#include "nvh/cameramanipulator.hpp"
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
        for (uint32_t i = 1; i < this->Count; ++i)
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

    std::vector<glm::u8vec4> getLookUpData(uint32_t sampleNum)
    {
        std::vector<glm::u8vec4> data(sampleNum);
        for (size_t index = 0; index < sampleNum; index++)
        {
            glm::vec3  v = this->Evaluate(index / static_cast<float>(sampleNum - 1));
            const auto x = static_cast<uint8_t>(std::round(255.0f * v.x));
            const auto y = static_cast<uint8_t>(std::round(255.0f * v.y));
            const auto z = static_cast<uint8_t>(std::round(255.0f * v.z));
            data[index]  = glm::u8vec4(x, y, z, static_cast<uint8_t>(0));
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

class Buffer;
struct ComputePass
{
   private:
    std::vector<Texture*> outputComponent;
    nvvk::PushComputeDispatcher<void>               dispatcher;
    std::vector<uint8_t> shaderCode;
    std::vector<Texture*> inputTextures;
    std::vector<Buffer*> inputBuffers;
    struct layoutState
    {
        VkImageLayout initlayout;
        VkImageLayout finallayout;
        bool needClear;
    };
    std::vector<layoutState> layoutStates;
    std::vector<VkDescriptorType> bufferTypes;
    PlayApp* app;

   public:
    ComputePass() = default;
    ComputePass(PlayApp* _app);
    ComputePass& addInputTexture(Texture*);
    ComputePass& addInputBuffer(Buffer* buffer, VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    ComputePass&                  addComponent(Texture* texture,VkImageLayout initLayout=VK_IMAGE_LAYOUT_UNDEFINED,VkImageLayout finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bool needClear = true);
    void setShaderCode(const std::string& filename);
    void setShaderCode(const std::vector<uint8_t>& code);
    void reset(){
        inputTextures.clear();
        inputBuffers.clear();
        outputComponent.clear();
        layoutStates.clear();
        bufferTypes.clear();
        shaderCode.clear();
    }
    void                  build(PlayApp* app, bool needCreatePipeline = true);
    void                  beginPass(VkCommandBuffer cmd);
    void                  endPass(VkCommandBuffer cmd);
    void dispatch(VkCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth);
    void destroy();
}; 

struct Scene;
class PlayApp;
class Texture;
class VolumeTexture;
class VolumeRenderer : public Renderer
{
    public:
    enum TextureBinding : uint32_t
    {
        eVolumeTexture = 0,
        eGradientTexture,
        eDiffuseLookUpTexture,
        eSpecularLookUpTexture,
        eRoughnessLookUpTexture,
        eOpacityLookUpTexture,
        eEnvTexture,
        eDiffuseRT,
        eSpecularRT,
        eRadianceRT,
        eNormalRT,
        eAccumulateRT,
        ePostProcessRT,
        eDepthRT,
        eCount
    };
    explicit VolumeRenderer(PlayApp& app);
    ~VolumeRenderer() override = default;
    void OnPreRender() override;
    void OnPostRender() override;
    void RenderFrame() override;
    void SetScene(Scene* scene) override;
    void OnResize(int width, int height) override;
    void OnDestroy() override;
    Texture* getOutputTexture() override
    {
        return _textureSlot[ePostProcessRT];
    };
    PlayApp* getApp(){return _app;}
    std::array<Texture*, static_cast<uint32_t>(TextureBinding::eCount)> getTextureSlot(){return _textureSlot;}
   protected:
    void loadVolumeTexture(const std::string& filename);
    void createRenderResource();
    void createRenderTarget();
    bool checkResourceState();
    void createComputePasses();

   private:
    friend struct ComputePass;
    struct VolumeUniform
    {
        glm::mat4 ProjectMatrix;
        glm::mat4 ViewMatrix;
        glm::mat4 WorldMatrix;
        glm::mat4 NormalMatrix;

        glm::mat4 InvProjectMatrix;
        glm::mat4 InvViewMatrix;
        glm::mat4 InvWorldMatrix;
        glm::mat4 InvNormalMatrix;

        uint32_t  frameCount;
        float     StepSize;
        glm::vec2 FrameOffset;

        glm::vec2 RTDimensions;
        float     Density;
        float     Exposure;
        glm::vec3 CameraPos;
        glm::vec3 BBoxMin;
        glm::vec3 BBoxMax;
    } _vUniform;

    std::array<Texture*, static_cast<uint32_t>(TextureBinding::eCount)> _textureSlot;
  
    PlayApp* _app;

    ComputePass* _generateRaysPass = nullptr;
    ComputePass* _radiancesPass= nullptr;
    ComputePass* _accumulatePass= nullptr;
    ComputePass* _postProcessPass= nullptr;
    ComputePass* _gradiantPass= nullptr;

    Buffer* _renderUniformBuffer;


    ColorTransferFunction1D  _diffuseTransferFunc;
    ColorTransferFunction1D  _specularTransferFunc;
    ScalarTransferFunction1D _roughnessTransferFunc;
    ScalarTransferFunction1D _opacityTransferFunc;
    uint32_t                 _frameCount;
    float                                             _density   = 100.0f;
    float                                             _exposure  = 1.0f;
    uint32_t                                          _stepCount = 180;
    glm::vec3                                         _BBoxMin   = glm::vec3(-0.5f,-0.5f,-0.65f);
    glm::vec3                                         _BBoxMax   = glm::vec3(0.5f,0.5f,0.65f);
    nvh::CameraManipulator::Camera                    _dirtyCamera;
};

} // namespace Play
#endif // VOLUMERENDERER_H