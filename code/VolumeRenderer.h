#ifndef VOLUMERENDERER_H
#define VOLUMERENDERER_H
#include "Renderer.h"
#include "glm/glm.hpp"

namespace Play
{
struct Scene;
struct PlayApp;
struct Texture;
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

    Texture* _volumeTexture = nullptr;
    Texture* _gradientTexture    = nullptr;
    Texture* _diffuseLookUpTexture = nullptr;
    Texture* _specularLookUpTexture = nullptr;
    Texture* _roughnessLookUpTexture = nullptr;
    Texture* _opacityLookUpTexture   = nullptr;

    Texture* _diffuseRT = nullptr;
    Texture* _specularRT = nullptr;
    Texture* _radianceRT = nullptr;
    Texture* _normalRT   = nullptr;
    Texture* _depthRT    = nullptr;

    Texture* _accumulateRT = nullptr;
    Texture* _postProcessRT = nullptr;

    PlayApp* _app;

};

} // namespace Play
#endif // VOLUMERENDERER_H