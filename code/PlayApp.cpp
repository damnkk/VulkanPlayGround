#include "PlayApp.h"
#include "nvvk/debug_util.hpp"
#include "nvvk/check_error.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_vulkan.h>
#include "stb_image.h"
#include "DeferRendering.h"
#include "resourceManagement/Resource.h"
#include "ShaderManager.hpp"
namespace Play
{
struct ScopeTimer
{
    std::chrono::high_resolution_clock::time_point start;
    ScopeTimer() : start(std::chrono::high_resolution_clock::now()) {}
    ~ScopeTimer()
    {
        auto end     = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Scene traversal time: " << elapsed.count() << " ms" << std::endl;
    }
};

void PlayElement::onAttach(nvapp::Application* app)
{
    _app = app;
    // _modelLoader.init(this);
    // CameraManip
    PlayResourceManager::Instance().initialize(this);
    TexturePool::Instance().init(65535, &PlayResourceManager::Instance());
    BufferPool::Instance().init(65535, &PlayResourceManager::Instance());
    ShaderManager::Instance().init(this);
    _frameData.resize(_app->getFrameCycleSize());
    for (size_t i = 0; i < _frameData.size(); ++i)
    {
        VkCommandPoolCreateInfo cmdPoolCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolCI.queueFamilyIndex = _app->getQueue(0).familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_app->getDevice(), &cmdPoolCI, nullptr,
                                       &_frameData[i].graphicsCmdPool));
        cmdPoolCI.queueFamilyIndex = _app->getQueue(1).familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_app->getDevice(), &cmdPoolCI, nullptr,
                                       &_frameData[i].computeCmdPool));
        VkSemaphoreTypeCreateInfo timelineCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue  = 0;

        VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphoreCreateInfo.flags = 0;
        semaphoreCreateInfo.pNext = &timelineCreateInfo;

        NVVK_CHECK(vkCreateSemaphore(_app->getDevice(), &semaphoreCreateInfo, nullptr,
                                     &_frameData[i].semaphore));
    }

    _profilerTimeline = _info.profilerManager->createTimeline({"graphics"});
    _profilerGpuTimer.init(_profilerTimeline, app->getDevice(), app->getPhysicalDevice(),
                           app->getQueue(0).familyIndex, true);
    createGraphicsDescriptResource();

    switch (_renderMode)
    {
        case eDeferRendering:
        {
            _renderer = std::make_shared<DeferRenderer>(*this);
            break;
        }
        default:
        {
            LOGE("Unsupported render mode, defaulting to DeferRendering");
            _renderer = std::make_shared<DeferRenderer>(*this);
        }
    }
}

void PlayElement::onDetach()
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    ImGui_ImplVulkan_RemoveTexture(_uiTextureDescriptor);
    TexturePool::Instance().deinit();
    BufferPool::Instance().deinit();
    PlayResourceManager::Instance().deInit();
    ShaderManager::Instance().deInit();
    _profilerGpuTimer.deinit();
    _info.profilerManager->destroyTimeline(_profilerTimeline);
    for (auto& frame : _frameData)
    {
        vkDestroySemaphore(_app->getDevice(), frame.semaphore, nullptr);
        vkDestroyCommandPool(_app->getDevice(), frame.graphicsCmdPool, nullptr);
        vkDestroyCommandPool(_app->getDevice(), frame.computeCmdPool, nullptr);
    }
}

void PlayElement::onResize(VkCommandBuffer cmd, const VkExtent2D& size)
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    _renderer->OnResize(size.width, size.height);
    ImGui_ImplVulkan_RemoveTexture(_uiTextureDescriptor);
    Texture::Destroy(_uiTexture);
    createGraphicsDescriptResource();
}

void PlayElement::onUIRender()
{
    ImGui::Begin("Viewport");
    ImGui::Image((ImTextureID) _uiTextureDescriptor, ImGui::GetContentRegionAvail());
    ImGui::End();
}

void PlayElement::onUIMenu() {}

void PlayElement::onPreRender()
{
    _renderer->OnPreRender();
}

void PlayElement::onRender(VkCommandBuffer cmd)
{
    VkClearColorValue       clearColor = {{0.91f, 0.23f, 0.77f, 1.0f}};
    VkImageSubresourceRange range      = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    nvvk::cmdImageMemoryBarrier(cmd, {_uiTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL});
    vkCmdClearColorImage(cmd, _uiTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor,
                         1, &range);
    nvvk::cmdImageMemoryBarrier(cmd, {_uiTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    PlayFrameData& frameData = _frameData[_app->getFrameCycleIndex()];
    frameData.reset(getDevice());

    _renderer->RenderFrame();
    _renderer->OnPostRender();
}

void PlayElement::onFileDrop(const std::filesystem::path& filename)
{
    // Handle file drop events here
    LOGI("File dropped: %s", filename.string().c_str());
}

void PlayElement::onLastHeadlessFrame()
{
    // Handle last frame in headless mode
    LOGI("Last headless frame");
}

void PlayElement::createGraphicsDescriptResource()
{
    _uiTexture = Texture::Create(_app->getWindowSize().width, _app->getWindowSize().height,
                                 VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    PlayResourceManager::Instance().acquireSampler(_uiTexture->descriptor.sampler);
    _uiTextureDescriptor = ImGui_ImplVulkan_AddTexture(_uiTexture->descriptor.sampler,
                                                       _uiTexture->descriptor.imageView,
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

std::filesystem::path getBaseFilePath()
{
    return "./";
}

// void PlayApp::OnInit()
// {
//     // NsightDebugger::initInjection();
//     _modelLoader.init(this);
//     CameraManip.setWindowSize(this->getSize().width, this->getSize().height);
//     // CameraManip.setMode(nvh::CameraManipulator::Modes::Fly);
//     CameraManip.setFov(120.0f);
//     CameraManip.setLookat({10.0, 10.0, 10.0}, {0.0, 0.0, 0.0}, {0.000, 1.000, 0.000});
//     CameraManip.setSpeed(10.0f);
//     // CameraManip
//     m_debug.setup(m_device);
//     _alloc.init(m_instance, m_device, m_physicalDevice);
//     _texturePool.init(65535, &_alloc);
//     _bufferPool.init(65535, &_alloc);

//     _modelLoader.loadModel(".\\resource\\models\\DamagedHelmet/DamagedHelmet.gltf");
//     //
//     _modelLoader.loadModel("D:\\repo\\DogEngine\\models\\MetalRoughSpheres\\MetalRoughSpheres.gltf");
//     // _modelLoader.loadModel("D:\\repo\\glTF-Sample-Models\\2.0\\ToyCar\\glTF\\ToyCar.gltf");

//     VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
//     VkDescriptorPoolSize       poolSize[] = {
//         {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1024},
//         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024},
//         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
//         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024},
//         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
//         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024},
//     };

//     poolInfo.poolSizeCount = arraySize(poolSize);
//     poolInfo.pPoolSizes    = poolSize;
//     poolInfo.maxSets       = 4096;
//     poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
//     vkCreateDescriptorPool(this->m_device, &poolInfo, nullptr, &_descriptorPool);
//     NAME_VK(_descriptorPool);
//     switch (_renderMode)
//     {
//         case eRayTracing:
//         {
//             _renderer = std::make_unique<RTRenderer>(*this);
//             break;
//         }
//         case eVolumeRendering:
//         {
//             _renderer = std::make_unique<VolumeRenderer>(*this);
//             break;
//         }
//         case eShadingRateRendering:
//         {
//             _renderer = std::make_unique<ShadingRateRenderer>(*this);
//             break;
//         }
//         default:
//         {
//             _renderer = std::make_unique<RTRenderer>(*this);
//             break;
//         }
//     }

//     createGraphicsDescriptResource();
//     createGraphicsPipeline();
// }
// void PlayApp::OnPreRender()
// {
//     _renderer->OnPreRender();
// }

// inline float luminance(const float* color)
// {
//     return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
// }

// void PlayApp::RenderFrame()
// {
//     auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
//     VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//     beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//     vkBeginCommandBuffer(cmd, &beginInfo);
//     _renderer->RenderFrame();
// }

// void PlayApp::createGraphicsDescriptResource()
// {
//     createGraphicsDescriptorSetLayout();
//     createGraphicsDescriptorSet();
// }
// void PlayApp::createGraphicsDescriptorSet()
// {
//     _graphicDescriptorSet =
//         nvvk::allocateDescriptorSet(m_device, _descriptorPool, _graphicsSetLayout);
//     VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//     writeDescriptorSet.dstSet          = _graphicDescriptorSet;
//     writeDescriptorSet.dstBinding      = 0;
//     writeDescriptorSet.dstArrayElement = 0;
//     auto tempDescriptor                = _renderer->getOutputTexture()->descriptor;
//     tempDescriptor.imageLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//     writeDescriptorSet.pImageInfo      = &tempDescriptor;
//     writeDescriptorSet.descriptorCount = 1;
//     writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//     vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
// }
// void PlayApp::createGraphicsDescriptorSetLayout()
// {
//     nvvk::DescriptorSetBindings bindings;
//     bindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
//                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
//     _graphicsSetLayout = bindings.createLayout(m_device, 0, nvvk::DescriptorSupport::CORE_1_2);
// }
// void PlayApp::createGraphicsPipeline()
// {
//     VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
//     pipelineLayoutInfo.setLayoutCount = 1;
//     pipelineLayoutInfo.pSetLayouts    = &_graphicsSetLayout;
//     vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &_graphicsPipelineLayout);
//     nvvk::GraphicsPipelineGeneratorCombined gpipelineState(m_device, _graphicsPipelineLayout,
//                                                            m_renderPass);
//     gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//     gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
//     gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
//     gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
//     VkViewport viewport{0.0f, 0.0f, (float) getSize().width, (float) getSize().height,
//     0.0f, 1.0f}; std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
//                                                  VK_DYNAMIC_STATE_SCISSOR};
//     gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
//     for (int i = 0; i < dynamicStates.size(); ++i)
//     {
//         gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
//     }
//     nvvk::ShaderModuleID vID = ShaderManager::registeShader(VK_SHADER_STAGE_VERTEX_BIT,
//     "shaders/post.vert", "main", ""); nvvk::ShaderModuleID fID =
//     ShaderManager::registeShader(VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/outputBlit.frag", "main",
//     "");

//     gpipelineState.addShader(ShaderManager::Instance().get(vID), VK_SHADER_STAGE_VERTEX_BIT,
//                              "main");
//     gpipelineState.addShader(ShaderManager::Instance().get(fID), VK_SHADER_STAGE_FRAGMENT_BIT,
//                              "main");

//     _graphicsPipeline = gpipelineState.createPipeline();
// }
// VkRenderPass PlayApp::GetOrCreateRenderPass(std::vector<RTState>& rtStates){
//     std::size_t key = 0;
//     for(const auto& state : rtStates)
//     {
//         std::size_t tempKey =
//         nvh::hashVal(state._loadOp,state._storeOp,state._texture->_metadata._format,state._resolveTexture->_metadata._format);
//         nvh::hashCombine(key, tempKey);
//     }
//     if(_renderPassesCache.find(key) != _renderPassesCache.end()) return _renderPassesCache[key];
//     VkRenderPassCreateInfo2 rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2};
//     std::vector<VkAttachmentDescription2> renderAttachments;
//     VkAttachmentReference2 depthReference{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
//     for(int i = 0;i<rtStates.size();++i){
//         const auto& state = rtStates[i];
//         auto& attachmentDesc =
//         renderAttachments.emplace_back(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
//         attachmentDesc.format = state._texture->getFormat();
//         attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
//         attachmentDesc.loadOp = state.getVkLoadOp();
//         attachmentDesc.storeOp = state.getVkStoreOp();
//         attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//         attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//         attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         if (state._resolveTexture)
//         {
//             auto& resolveAttachmentDesc =
//             renderAttachments.emplace_back(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
//             resolveAttachmentDesc.format = state._resolveTexture->getFormat();
//             resolveAttachmentDesc.samples = state._resolveTexture->getSampleCount();
//             resolveAttachmentDesc.loadOp = state.getVkLoadOp();
//             resolveAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//             resolveAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//             resolveAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//             resolveAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//             resolveAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//         }
//         if(state._texture->isDepth()){
//             depthReference.attachment = renderAttachments.size()-1;
//             depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//         }
//     }

//     std::vector<VkAttachmentReference2> attachmentReferences;
//     std::vector<VkAttachmentReference2> resolveAttachmentReferences;
//     for(int i = 0;i<rtStates.size();++i){
//         RTState& state = rtStates[i];
//         if(state._texture->isDepth()){
//             continue;
//         }
//        VkAttachmentReference2& attachmentRef =
//        attachmentReferences.emplace_back(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
//        attachmentRef.attachment = attachmentReferences.size()-1;
//        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//        if(state._resolveTexture)
//        {
//             VkAttachmentReference2 resolveAttachmentRef =
//             resolveAttachmentReferences.emplace_back(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
//            resolveAttachmentRef.attachment = resolveAttachmentReferences.size()-1;
//            resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//        }else{
//             VkAttachmentReference2& resolveAttachmentRef =
//             resolveAttachmentReferences.emplace_back(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
//            resolveAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;
//            resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
//        }
//     }
//     VkSubpassDescription2 subpassDescription{VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2};
//     subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//     subpassDescription.inputAttachmentCount = 0;
//     subpassDescription.pInputAttachments = nullptr;
//     subpassDescription.colorAttachmentCount = static_cast<uint32_t>(attachmentReferences.size());
//     subpassDescription.pColorAttachments = attachmentReferences.data();
//     subpassDescription.pResolveAttachments = resolveAttachmentReferences.data();
//     subpassDescription.pDepthStencilAttachment = &depthReference;
//     subpassDescription.pPreserveAttachments = nullptr;

//     VkSubpassDependency2 dependencies[2];
//     dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
//     dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
//     dependencies[0].dstSubpass = 0;
//     dependencies[0].srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
//     dependencies[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
//     dependencies[0].srcAccessMask = VK_ACCESS_2_NONE;
//     dependencies[0].dstAccessMask =
//     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

//     dependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
//     dependencies[1].srcSubpass = 1;
//     dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
//     dependencies[1].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
//     dependencies[1].dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
//     dependencies[1].srcAccessMask =
//     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//     dependencies[1].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

//     rpci.attachmentCount = renderAttachments.size();
//     rpci.pAttachments = renderAttachments.data();
//     rpci.dependencyCount = 2;
//     rpci.pDependencies = dependencies;
//     rpci.subpassCount = 1;
//     rpci.pSubpasses = &subpassDescription;

//     VkRenderPass rp;
//     NV_ASSERT(vkCreateRenderPass2(getDevice(), &rpci, nullptr, &rp));
//     _renderPassesCache.emplace(key,rp);
//     return rp;
// }

// VkPipeline PlayApp::GetOrCreatePipeline(const ShaderInfo* computeShaderInfo){
//     // std::size_t computePiplineHash = computeShaderInfo->getHash();
//     // if(_pipelineCache.find(computePiplineHash) != _pipelineCache.end())
//     //     return _pipelineCache[computePiplineHash];
//     // VkComputePipelineCreateInfo computePplci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
//     return VK_NULL_HANDLE;
// }

// void PlayApp::OnPostRender()
// {
//     _renderer->OnPostRender();
//     auto         cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
//     VkClearValue clearValue[2];
//     clearValue[0].color        = {{0.0f, 1.0f, 1.0f, 1.0f}};
//     clearValue[1].depthStencil = {1.0f, 0};
//     VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
//     renderPassBeginInfo.renderPass = this->getRenderPass();
//     renderPassBeginInfo.framebuffer =
//         this->getFramebuffers()[this->m_swapChain.getActiveImageIndex()];
//     renderPassBeginInfo.renderArea      = VkRect2D({0, 0}, this->getSize());
//     renderPassBeginInfo.clearValueCount = 2;
//     renderPassBeginInfo.pClearValues    = clearValue;
//     vkCmdBeginRenderPass(cmd, &renderPassBeginInfo,
//     VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(cmd,
//     VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline); VkViewport viewport{0.0f, 0.0f, (float)
//     getSize().width, (float) getSize().height, 0.0f, 1.0f}; vkCmdSetViewport(cmd, 0, 1,
//     &viewport); VkRect2D scissor = {{0, 0}, this->getSize()}; vkCmdSetScissor(cmd, 0, 1,
//     &scissor); vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
//     _graphicsPipelineLayout, 0, 1,
//                             &_graphicDescriptorSet, 0, nullptr);
//     vkCmdDraw(cmd, 3, 1, 0, 0);
//     ImGui_ImplVulkan_NewFrame();
//     ImGui_ImplGlfw_NewFrame();
//     ImGui::NewFrame();
//     // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

//     ImGui::BeginMainMenuBar();
//     ImGui::MenuItem("File");
//     ImGui::EndMainMenuBar();

//     ImGui::Render();
//     if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
//     {
//         if (m_window)
//         {
//             ImGui::UpdatePlatformWindows();
//             ImGui::RenderPlatformWindowsDefault();
//             glfwMakeContextCurrent(m_window);
//         }
//     }
//     ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
//     vkCmdEndRenderPass(cmd);
//     vkEndCommandBuffer(cmd);
// }
// Texture* PlayApp::CreateTexture(VkImageCreateInfo& info, VkCommandBuffer* cmd)
// {
//     if (!cmd) *cmd = createTempCmdBuffer();
//     Texture*      texture     = _texturePool.alloc();
//     auto          samplerInfo = nvvk::makeSamplerCreateInfo();
//     nvvk::Texture nvvkTexture =
//         _alloc.createTexture(*cmd, 0, nullptr, info, samplerInfo, VK_IMAGE_LAYOUT_UNDEFINED);
//     texture->image      = nvvkTexture.image;
//     texture->memHandle  = nvvkTexture.memHandle;
//     texture->descriptor = nvvkTexture.descriptor;
//     texture->_metadata._format    = info.format;
//     NAME_VK(texture->image);
//     submitTempCmdBuffer(*cmd);
//     return texture;
// }
// Buffer* PlayApp::CreateBuffer(VkBufferCreateInfo& info, VkMemoryPropertyFlags memProperties)
// {
//     Buffer*      buffer     = _bufferPool.alloc();
//     nvvk::Buffer nvvkBuffer = _alloc.createBuffer(info, memProperties);
//     buffer->buffer          = nvvkBuffer.buffer;
//     buffer->address         = nvvkBuffer.address;
//     buffer->memHandle       = nvvkBuffer.memHandle;
//     NAME_VK(buffer->buffer);
//     return buffer;
// }
// void PlayApp::onResize(int width, int height)
// {
//     vkQueueWaitIdle(this->m_queue);
//     _renderer->OnResize(width, height);

//     // update graphics descriptor set
//     VkDescriptorImageInfo imageInfo;
//     auto                  tempDescriptor = _renderer->getOutputTexture()->descriptor;
//     imageInfo.imageLayout                = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//     imageInfo.imageView                  = tempDescriptor.imageView;
//     imageInfo.sampler                    = tempDescriptor.sampler;
//     VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//     writeDescriptorSet.dstSet          = _graphicDescriptorSet;
//     writeDescriptorSet.dstBinding      = 0;
//     writeDescriptorSet.dstArrayElement = 0;
//     writeDescriptorSet.pImageInfo      = &imageInfo;
//     writeDescriptorSet.descriptorCount = 1;
//     writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//     vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
// }
// void PlayApp::Run()
// {
//     while (!glfwWindowShouldClose(m_window))
//     {
//         glfwPollEvents();
//         if (this->isMinimized())
//         {
//             continue;
//         }
//         this->prepareFrame();
//         this->OnPreRender();
//         this->RenderFrame();
//         this->OnPostRender();
//         this->submitFrame();
//     }
// }

// void PlayApp::onDestroy()
// {
//     vkDeviceWaitIdle(m_device);
//     _renderer->OnDestroy();
//     this->_texturePool.deinit();
//     this->_bufferPool.deinit();
//     vkDestroyDescriptorPool(m_device, this->_descriptorPool, nullptr);
//     _alloc.deinit();
//     // for(auto& [createInfo, renderPass] : _renderPassesCache) {
//     //     if(renderPass == VK_NULL_HANDLE) continue;
//     //     vkDestroyRenderPass(m_device, renderPass, nullptr);
//     // }
// }
} // namespace Play