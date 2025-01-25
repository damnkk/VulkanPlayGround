#include "PlayApp.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"

#include "nvh/nvprint.hpp"

#include "imgui/imgui_camera_widget.h"
#include "nvp/perproject_globals.hpp"

namespace Play
{

void PlayApp::OnInit()
{
    _modelLoader.init(this);
    _modelLoader.loadModel("F:/repository/ModelResource/Bistro_v5_2/BistroInterior.fbx");
    _alloc.init(m_instance, m_device, m_physicalDevice);
    _texturePool.init(2048, &_alloc);
    _bufferPool.init(4096, &_alloc);
}
void PlayApp::OnPreRender() {}

void PlayApp::RenderFrame() {}

void PlayApp::OnPostRender()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    VkClearValue clearValue[2];
    clearValue[0].color        = {{0.0f, 1.0f, 0.0f, 1.0f}};
    clearValue[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBeginInfo.renderPass = this->getRenderPass();
    renderPassBeginInfo.framebuffer =
        this->getFramebuffers()[this->m_swapChain.getActiveImageIndex()];
    renderPassBeginInfo.renderArea      = VkRect2D({0, 0}, this->getSize());
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues    = clearValue;
    vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
    ImGui::BeginMainMenuBar();
    ImGui::MenuItem("File");
    ImGui::EndMainMenuBar();
    // ImGui::ShowDemoWindow();
    ImGui::Render();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        if (m_window)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(m_window);
        }
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

void PlayApp::onResize(int width, int height)
{
    // vkDeviceWaitIdle(this->m_device);
    // this->m_size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    // vkDestroySwapchainKHR(this->m_device, this->m_swapChain.getSwapchain(), nullptr);
    // this->createSwapchain(this->m_surface, width, height);
    // for (auto& framebuffer : this->m_framebuffers)
    // {
    //     vkDestroyFramebuffer(this->m_device, framebuffer, nullptr);
    // }
    // this->createFrameBuffers();
}
void PlayApp::Run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        if (this->isMinimized())
        {
            continue;
        }
        this->prepareFrame();
        this->OnPreRender();
        this->RenderFrame();
        this->OnPostRender();
        this->submitFrame();
    }
}
} // namespace Play