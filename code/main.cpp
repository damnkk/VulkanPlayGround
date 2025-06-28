#include "glfw/glfw3.h"
#include "PlayApp.h"
#include "nvvk/context_vk.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "resourceManagement/shaderManager.h"
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
        ctxCreateInfo.addInstanceExtension(instanceExtName[i],
                                           false); // 获取glfw所需的扩展(surface扩展)
    }
    ctxCreateInfo.addInstanceLayer("VK_LAYER_KHRONOS_profiles", false); // 暂不使用

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
        // 物理硬件扩展，需要一个feature structure
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    ctxCreateInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false,
                                     &rtPipelineFeature);
    VkPhysicalDeviceShaderClockFeaturesKHR shaderClockFeature{
        // 物理硬件扩展，需要一个feature structure
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR};
    ctxCreateInfo.addDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME, false,
                                     &shaderClockFeature);
    ctxCreateInfo.addDeviceExtension("VK_KHR_portability_subset", false);
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    ctxCreateInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false,
                                     &accelFeature); // To build acceleration structures
    ctxCreateInfo.addDeviceExtension("VK_KHR_deferred_host_operations", false);
    ctxCreateInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, false);
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeature;
    fragmentShadingRateFeature.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
    fragmentShadingRateFeature.attachmentFragmentShadingRate = true;
    fragmentShadingRateFeature.pipelineFragmentShadingRate = true;
    fragmentShadingRateFeature.primitiveFragmentShadingRate = true;
    ctxCreateInfo.addDeviceExtension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, false,
                                     &fragmentShadingRateFeature);
    VkValidationFeatureEnableEXT validationFeatureToEnable =
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
    VkValidationFeaturesEXT validationInfo       = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validationInfo.enabledValidationFeatureCount = 1;
    validationInfo.pEnabledValidationFeatures    = &validationFeatureToEnable;
    ctxCreateInfo.instanceCreateInfoExt          = &validationInfo;
#if WIN32
    _putenv_s("VK_LAYER_PRINTF_TO_STDOUT", "1");
    _putenv_s("VK_LAYER_PRINTF_BUFFER_SIZE", "8192");
#endif // WIN32
    ctxCreateInfo.addDeviceExtension("VK_KHR_spirv_1_4", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_swapchain", false);
    ctxCreateInfo.addDeviceExtension("VK_KHR_ray_query", true);
    ctxCreateInfo.addDeviceExtension(("VK_KHR_push_descriptor"), false);

    nvvk::Context vkCtx;
    vkCtx.init(ctxCreateInfo);
    Play::ShaderManager::initialize();

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
    app->onDestroy();
    return 0;
}