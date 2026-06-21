
#define VMA_IMPLEMENTATION

#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/context.hpp>
#include <nvvk/debug_util.hpp>
#include <nvvk/default_structs.hpp>
#include <nvvk/resource_allocator.hpp>
#include <nvvk/sampler_pool.hpp>
#include <nvvk/staging.hpp>
#include <nvutils/parameter_parser.hpp>
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
        // std::string renderMode = "volume";
        // std::string renderMode = "gaussian";
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
            .renderMode  = renderMode,
        };

        auto afterMathExtList = Play::NsightDebugger::initInjection();

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
        return engineLoop.run(runtimeConfig, vkSetup);
    }
}
