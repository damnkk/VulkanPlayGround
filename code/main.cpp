#include "glfw/glfw3.h"
#include "PlayApp.h"
#include "nvvk/context_vk.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "backends/imgui_impl_glfw.h"

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        return -1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Vulkan Play Ground", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    uint32_t     glfwNeededInstanceExtensionCount;
    const char** instanceExtName =
        glfwGetRequiredInstanceExtensions(&glfwNeededInstanceExtensionCount);

    std::vector<VkExtensionProperties> InstanceExtensions;
    uint32_t                           InstanceExtensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &InstanceExtensionCount, nullptr);
    InstanceExtensions.resize(InstanceExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &InstanceExtensionCount,
                                           InstanceExtensions.data());

    nvvk::ContextCreateInfo ctxCreateInfo;
    ctxCreateInfo.setVersion(1, 3);

    for (uint32_t i = 0; i < glfwNeededInstanceExtensionCount; i++)
    {
        ctxCreateInfo.addInstanceExtension(instanceExtName[i], false);
    }
    ctxCreateInfo.addInstanceLayer("VK_LAYER_KHRONOS_profiles", false);

    ctxCreateInfo.addDeviceExtension("VK_KHR_ray_tracing_pipeline", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_portability_subset", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_acceleration_structure", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_deferred_host_operations", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_spirv_1_4", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_swapchain", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_ray_query", false);

    nvvk::Context vkCtx;
    vkCtx.init(ctxCreateInfo);

    Play::PlayApp*              app = new Play::PlayApp();
    nvvkhl::AppBaseVkCreateInfo appInfo;
    appInfo.physicalDevice = vkCtx.m_physicalDevice;
    appInfo.device         = vkCtx.m_device;
    appInfo.instance       = vkCtx.m_instance;
    appInfo.window         = window;
    appInfo.size           = {1920, 1080};
    appInfo.queueIndices   = {vkCtx.m_queueGCT.familyIndex, vkCtx.m_queueC.familyIndex,
                              vkCtx.m_queueT.familyIndex};
    appInfo.surface        = app->getVkSurface(appInfo.instance, window);
    app->create(appInfo);
    app->OnInit();
    app->Run();
    app->destroy();
    return 0;
}