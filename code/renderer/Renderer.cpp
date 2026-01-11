#include "Renderer.h"
#include "Resource.h"
#include "core/PlayCamera.h"
#include "SceneManager.h"
#include "VulkanDriver.h"
namespace Play
{

void Renderer::addCamera()
{
    auto camera = std::make_unique<PlayCamera>();
    camera->getCameraManipulator()->setMode(nvutils::CameraManipulator::Modes::Fly);
    _cameras.push_back(std::move(camera));
}

void Renderer::setActiveCamera(size_t index)
{
    _activeCameraIdx = index;
}

Buffer* Renderer::getCurrentCameraBuffer() const
{
    return _cameraUniformData[vkDriver->getFrameCycleIndex()];
}

Renderer::Renderer()
{
    addCamera();

    _scene = std::make_unique<SceneManager>();
    // _scene->addScene("D:/repo/downloaded_resources/man/SK_Man_Full_04.gltf");
    std::filesystem::path modelPath     = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
    Texture*              skyboxTexture = Texture::Create(modelPath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, true);
    _scene->addSkyBoxTexture(skyboxTexture);
    _scene->updateDescriptorSet();

    for (int i = 0; i < _cameraUniformData.size(); ++i)
    {
        _cameraUniformData[i] = Buffer::Create("cameraInfoBuf" + std::to_string(i), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(CameraData),
                                               VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
};

} // namespace Play