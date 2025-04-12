#include "VolumeRenderer.h"

#include "nvvk/images_vk.hpp"
#include "utils.hpp"
#include <PlayApp.h>
namespace Play
{
VolumeRenderer::VolumeRenderer(PlayApp& app)
{
    _app = &app;
    createRenderResource();
}
void VolumeRenderer::OnPreRender() {}
void VolumeRenderer::OnPostRender() {}
void VolumeRenderer::RenderFrame() {}
void VolumeRenderer::SetScene(Scene* scene) {}
void VolumeRenderer::OnResize(int width, int height) {}
void VolumeRenderer::OnDestroy() {}
void VolumeRenderer::loadVolumeTexture(const std::string& filename) {}
void VolumeRenderer::createRenderResource()
{
    // create volume texture
    loadVolumeTexture("data/volume/volume.raw");
    // create lookup texture
    // create multiple render target
    _diffuseRT = PlayApp::AllocTexture();
    _specularRT = PlayApp::AllocTexture();
    _radianceRT = PlayApp::AllocTexture();
    _normalRT = PlayApp::AllocTexture();
    _depthRT = PlayApp::AllocTexture();
    _accumulateRT = PlayApp::AllocTexture();
    _postProcessRT = PlayApp::AllocTexture();
    auto colorImageCreateInfo = nvvk::makeImage2DCreateInfo(_app->getSize(),VK_FORMAT_R16G16B16_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT,false);
    auto  samplerCreateInfo = nvvk::makeSamplerCreateInfo();
    auto cmd = _app->createTempCmdBuffer();
    std::function<void(Texture*,VkImageCreateInfo&)> createTexture = [&](Texture* texture,VkImageCreateInfo& imgInfo) {
        nvvk::Texture nvvkTexture = _app->_alloc.createTexture(cmd, 0, nullptr, imgInfo, samplerCreateInfo, VK_IMAGE_LAYOUT_UNDEFINED);
        texture->image        = nvvkTexture.image;
        texture->memHandle    = nvvkTexture.memHandle;
        texture->descriptor   = nvvkTexture.descriptor;
        texture->_format      = imgInfo.format;
        CUSTOM_NAME_VK(_app->m_debug, texture->image);
    };
    createTexture(_diffuseRT,colorImageCreateInfo);
    createTexture(_specularRT,colorImageCreateInfo);
    createTexture(_radianceRT,colorImageCreateInfo);
    createTexture(_normalRT,colorImageCreateInfo);
    auto depthImageCreateInfo = nvvk::makeImage2DCreateInfo(_app->getSize(),VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,false);
    createTexture(_depthRT,depthImageCreateInfo);
    auto accumulateImageCreateInfo = nvvk::makeImage2DCreateInfo(_app->getSize(),VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,false);
    createTexture(_accumulateRT,accumulateImageCreateInfo);
    auto postProcessImageCreateInfo = nvvk::makeImage2DCreateInfo(_app->getSize(),VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,false);
    createTexture(_postProcessRT,postProcessImageCreateInfo);



}
} // namespace Play
