#include "RTRenderer.h"
#include "SceneNode.h"
#include "PlayApp.h"
#include "nvvk/debug_util_vk.hpp"
#include "utils.hpp"
namespace Play
{
RTRenderer::RTRenderer(PlayApp& app)
{
    _app = &app;
    _rtBuilder.setup(app.m_device, &app._alloc, app.getQueueFamily());
    _rtBuilder.setup(app.m_device, &app._alloc, app.m_graphicsQueueIndex);
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(app.m_physicalDevice, &properties2);
    _shaderModuleManager.init(app.m_device, 1, 3);
    _sbtWrapper.setup(app.m_device, app.getQueueFamily(), &app._alloc, rtProperties);
    rayTraceRTCreate();
}
void RTRenderer::OnPreRender() {}
void RTRenderer::OnPostRender() {}
void RTRenderer::RenderFrame() {}
void RTRenderer::SetScene(Scene* scene){this->_scene = scene;}

void RTRenderer::loadEnvTexture(){}
void RTRenderer::buildTlas(){}
void RTRenderer::buildBlas(){}
void RTRenderer::createDescritorSet(){}
void RTRenderer::rayTraceRTCreate()
{
    Texture rayTraceRT;
    auto    textureCreateinfo = nvvk::makeImage2DCreateInfo(
        VkExtent2D{_app->getSize().width, _app->getSize().height}, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
    rayTraceRT             = PlayApp::AllocTexture();
    auto samplerCreateInfo = nvvk::makeSamplerCreateInfo(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    auto          cmd         = _app->createTempCmdBuffer();
    nvvk::Texture nvvkTexture = _app->_alloc.createTexture(cmd, 0, nullptr, textureCreateinfo,
                                                     samplerCreateInfo, VK_IMAGE_LAYOUT_GENERAL);
    _app->submitTempCmdBuffer(cmd);
    rayTraceRT.image        = nvvkTexture.image;
    rayTraceRT.memHandle    = nvvkTexture.memHandle;
    rayTraceRT.descriptor   = nvvkTexture.descriptor;
    rayTraceRT._format      = VK_FORMAT_R32G32B32A32_SFLOAT;
    rayTraceRT._mipmapLevel = textureCreateinfo.mipLevels;
    _rayTraceRT             = rayTraceRT;
    CUSTOM_NAME_VK(_app->m_debug,_rayTraceRT.image);
}
void RTRenderer::createRenderBuffer(){}
void RTRenderer::createGraphicsPipeline(){}
void RTRenderer::createRTPipeline(){}
void RTRenderer::createRazterizationRenderPass(){}
void RTRenderer::createRazterizationFBO(){}
void RTRenderer::createPostPipeline(){}
void RTRenderer::createPostDescriptorSet(){}

} // namespace Play