
#define VMA_IMPLEMENTATION

#include <backends/imgui_impl_vulkan.h>
#include <nvapp/application.hpp>
#include <nvapp/elem_profiler.hpp>
#include <nvapp/elem_logger.hpp>
#include <nvapp/elem_default_menu.hpp>
#include <nvapp/elem_default_title.hpp>
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/context.hpp>
#include <nvvk/debug_util.hpp>
#include <nvvk/default_structs.hpp>
#include <nvvk/resource_allocator.hpp>
#include <nvvk/sampler_pool.hpp>
#include <nvvk/staging.hpp>
#include <nvvk/profiler_vk.hpp>
#include <nvutils/parameter_parser.hpp>
#include "PlayApp.h"
#include "core/runtime/EngineLoop.h"
#include "debugger/debugger.h"


int main(int argc, char** argv)
{
    nvutils::Logger::getInstance().breakOnError(false);
    {
        bool validation = false;
        bool verbose    = false;

        nvutils::ParameterRegistry parameterRegistry;
        nvutils::ParameterParser   parameterParser;
        parameterRegistry.add({"validation"}, &validation);
        parameterRegistry.add({"verbose"}, &verbose);
        std::string renderMode = "defer";
        parameterRegistry.add({"rendermode", "rm"}, &renderMode);
        parameterParser.add(parameterRegistry);
        parameterParser.parse(argc, argv);

        Play::runtime::RuntimeConfig runtimeConfig{
            .windowTitle = "VulkanPlayGround SDL Runtime",
            .width       = 1280,
            .height      = 720,
            .vSync       = false,
            .validation  = validation,
            .verbose     = verbose,
        };

        auto afterMathExtList = Play::NsightDebugger::initInjection();

        VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr, VK_TRUE};
        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT, nullptr, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE};
        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
            .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .pNext      = nullptr,
            .taskShader = VK_TRUE,
            .meshShader = VK_TRUE,
        };
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsrFeatures = {
            .sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
            .pNext                         = nullptr,
            .pipelineFragmentShadingRate   = VK_TRUE,
            .primitiveFragmentShadingRate  = VK_TRUE,
            .attachmentFragmentShadingRate = VK_TRUE,
        };
        VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures = {
            .sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
            .pNext                     = nullptr,
            .fragmentShaderBarycentric = VK_TRUE,
        };

        nvvk::ContextInitInfo vkSetup{
            .instanceExtensions     = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
            .deviceExtensions =
                {
                    {VK_EXT_MESH_SHADER_EXTENSION_NAME, &meshShaderFeatures, true},
                    {VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, &fsrFeatures, true},
                    {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
                    {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME},
                    {VK_KHR_RAY_QUERY_EXTENSION_NAME, &rayQueryFeatures},
                    {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME},
                    {VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME},
                    {VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME, &descriptorBufferFeatures},
                },
            .queues                 = {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT},
            .applicationName        = runtimeConfig.windowTitle,
            .enableValidationLayers = runtimeConfig.validation,
            .verbose                = runtimeConfig.verbose,
        };
        vkSetup.deviceExtensions.insert(vkSetup.deviceExtensions.end(), afterMathExtList.begin(), afterMathExtList.end());
        if (renderMode == "gaussian")
        {
            vkSetup.deviceExtensions.push_back({VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, &barycentricFeatures});
        }

        Play::runtime::EngineLoop engineLoop;
        engineLoop.run(runtimeConfig, vkSetup);
        return 0;
    }

    auto                       afterMathExtList = Play::NsightDebugger::initInjection();
    nvutils::ProfilerManager   profilerManager;
    nvutils::ParameterRegistry parameterRegistry;
    nvutils::ParameterParser   parameterParser;

    // Command-line render mode configuration
    // std::string renderMode = "gaussian";
    std::string renderMode = "defer";
    parameterRegistry.add({"rendermode", "rm"}, &renderMode);

    Play::RenderSession::Info playInfo = {
        .profilerManager   = &profilerManager,
        .parameterRegistry = &parameterRegistry,
        .renderMode        = &renderMode,
    };
    // setup logger element, `true` means shown by default
    // we add it early so outputs are captured early on, you might want to defer this to a later
    // timer.
    std::shared_ptr<nvapp::ElementLogger> elementLogger = std::make_shared<nvapp::ElementLogger>(true);
    nvutils::Logger::getInstance().setLogCallback([&](nvutils::Logger::LogLevel logLevel, const std::string& text)
                                                  { elementLogger->addLog(logLevel, "%s", text.c_str()); });
    VkPhysicalDeviceRayQueryFeaturesKHR         rayQueryFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr, VK_TRUE};
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT, nullptr, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE};
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
        .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext      = nullptr,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
    };
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsrFeatures = {
        .sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
        .pNext                         = nullptr,
        .pipelineFragmentShadingRate   = VK_TRUE,
        .primitiveFragmentShadingRate  = VK_TRUE,
        .attachmentFragmentShadingRate = VK_TRUE,
    };
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures = {
        .sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
        .pNext                     = nullptr,
        .fragmentShaderBarycentric = VK_TRUE,
    };
    nvvk::ContextInitInfo vkSetup{
        .instanceExtensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME},

        .deviceExtensions =
            {
                {VK_EXT_MESH_SHADER_EXTENSION_NAME, &meshShaderFeatures, true},
                {VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, &fsrFeatures, true},
                {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
                {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME},
                {VK_KHR_RAY_QUERY_EXTENSION_NAME, &rayQueryFeatures},
                {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME},
                {VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME},
                {VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME, &descriptorBufferFeatures},
            },
        .queues = {VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT},
    };
    vkSetup.deviceExtensions.insert(vkSetup.deviceExtensions.end(), afterMathExtList.begin(), afterMathExtList.end());

    // let's add a command-line option to enable/disable validation layers
    parameterRegistry.add({"validation"}, &vkSetup.enableValidationLayers);
    parameterRegistry.add({"verbose"}, &vkSetup.verbose);
    // as well as an option to force the vulkan device based on canonical index
    parameterRegistry.add({"forcedevice"}, &vkSetup.forceGPU);

    // add all parameters to the parser
    parameterParser.add(parameterRegistry);

    // and then parse command line
    parameterParser.parse(argc, argv);

    if (renderMode == "gaussian")
    {
        vkSetup.deviceExtensions.push_back({VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, &barycentricFeatures});
    }

    nvvk::addSurfaceExtensions(vkSetup.instanceExtensions);
    nvvk::Context vkContext;
    if (vkContext.init(vkSetup) != VK_SUCCESS)
    {
        LOGE("Error in Vulkan context creation\n");
        return 1;
    }

    nvapp::ApplicationCreateInfo appInfo;
    appInfo.name           = "The Empty Example";
    appInfo.useMenu        = true;
    appInfo.instance       = vkContext.getInstance();
    appInfo.device         = vkContext.getDevice();
    appInfo.physicalDevice = vkContext.getPhysicalDevice();
    appInfo.queues         = vkContext.getQueueInfos();
    appInfo.dockSetup      = [](ImGuiID viewportID)
    {
        // right side panel container
        ImGuiID settingID = ImGui::DockBuilderSplitNode(viewportID, ImGuiDir_Right, 0.25F, nullptr, &viewportID);
        ImGui::DockBuilderDockWindow("Settings", settingID);

        // bottom panel container
        ImGuiID loggerID = ImGui::DockBuilderSplitNode(viewportID, ImGuiDir_Down, 0.35F, nullptr, &viewportID);
        ImGui::DockBuilderDockWindow("Log", loggerID);
        ImGuiID profilerID = ImGui::DockBuilderSplitNode(loggerID, ImGuiDir_Right, 0.4F, nullptr, &loggerID);
        ImGui::DockBuilderDockWindow("Profiler", profilerID);
    };

    // Create the application
    nvapp::Application app;
    app.init(appInfo);

    // add the sample main element
    // app.addElement(sampleElement);
    app.addElement(std::make_shared<Play::RenderSession>(playInfo));
    app.addElement(std::make_shared<nvapp::ElementDefaultWindowTitle>());
    // add profiler element
    app.addElement(std::make_shared<nvapp::ElementProfiler>(&profilerManager));
    // add logger element
    app.addElement(elementLogger);

    LOGI("%s", "Wohoo let's run this sample!\n");

    // enter the main loop
    app.run();

    // Cleanup in reverse order
    app.deinit();
    vkContext.deinit();

    return 0;
}
