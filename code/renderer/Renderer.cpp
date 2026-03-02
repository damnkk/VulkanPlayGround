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

    for (int i = 0; i < _cameraUniformData.size(); ++i)
    {
        _cameraUniformData[i] = Buffer::Create("cameraInfoBuf" + std::to_string(i), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(CameraData),
                                               VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }
};

void Renderer::updateCameraBuffer()
{
    // update curr activated camera into camera buffer
    PlayCamera* camera = getActiveCamera();
    camera->update(ImGui::FindWindowByName("Viewport"));
    CameraData data{};
    data.cameraPosition    = camera->getCameraManipulator()->getEye();
    data.projMatrix        = camera->getCameraManipulator()->getPerspectiveMatrix();
    data.viewMatrix        = camera->getCameraManipulator()->getViewMatrix();
    data.viewPortSize      = {vkDriver->getApp()->getViewportSize().width, vkDriver->getApp()->getViewportSize().height};
    data.viewProjMatrix    = data.projMatrix * data.viewMatrix;
    data.invViewMatrix     = glm::inverse(data.viewMatrix);
    data.invProjMatrix     = glm::inverse(data.projMatrix);
    data.invViewProjMatrix = glm::inverse(data.viewProjMatrix);

    memcpy(getCurrentCameraBuffer()->mapping, &data, sizeof(CameraData));
    PlayResourceManager::Instance().flushBuffer(*getCurrentCameraBuffer(), 0, VK_WHOLE_SIZE);
}

} // namespace Play