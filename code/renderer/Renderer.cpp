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
    auto defaultCamera = std::make_unique<PlayCamera>();
    addCamera();
    _scene = std::make_unique<SceneManager>();
    // _scene->addScene("D:/repo/downloaded_resources/man/SK_Man_Full_04.gltf");
    _scene->updateDescriptorSet();

    for (int i = 0; i < _cameraUniformData.size(); ++i)
    {
        _cameraUniformData[i] = Buffer::Create("cameraInfoBuf" + std::to_string(i), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(CameraData),
                                               VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
};

} // namespace Play