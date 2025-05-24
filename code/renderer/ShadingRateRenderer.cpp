#include "ShadingRateRenderer.h"
#include "nvvk/images_vk.hpp"
#include <PlayApp.h>
#include "nvvk/buffers_vk.hpp"

#include <corecrt_io.h>
#include <utils.hpp>
namespace Play
{
 ShadingRateRenderer::ShadingRateRenderer(PlayApp& app) : Renderer(app)
 {
     createRenderPass();
     createRenderResource();
     createFrameBuffer();
     initPipeline();
 }
void                 ShadingRateRenderer::OnPreRender()
 {
     ShaderRateUniformStruct* data = static_cast<ShaderRateUniformStruct*>(PlayApp::MapBuffer(*_renderUniformBuffer));
     data->ProjectMatrix = glm::perspectiveFov(CameraManip.getFov(), _app->getSize().width * 1.0f,
                                               _app->getSize().height * 1.0f, 0.1f, 10000.0f);
     data->ProjectMatrix[1][1] *= -1;
     data->ViewMatrix     = CameraManip.getMatrix();
     data->WorldMatrix    = glm::mat4(1.0f);
     data->InvWorldMatrix = glm::inverse(data->WorldMatrix);

     data->InvViewMatrix    = glm::inverse(data->ViewMatrix);
     data->InvProjectMatrix = glm::inverse(data->ProjectMatrix);
     data->CameraPos        = CameraManip.getEye();

     PlayApp::UnmapBuffer(*_renderUniformBuffer);
     if (_dirtyCamera != CameraManip.getCamera())
     {
         _dirtyCamera = CameraManip.getCamera();
     }
 }
void                 ShadingRateRenderer::OnPostRender() {}
void                 ShadingRateRenderer::RenderFrame()
 {
     auto cmd = _app->getCommandBuffers()[_app->m_swapChain.getActiveImageIndex()];
     VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
     beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _razePipeline);

     VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
     renderPassBeginInfo.renderPass = _shadingRateRenderPass;
     renderPassBeginInfo.framebuffer = _shadingRateFramebuffer;
     renderPassBeginInfo.renderArea = {0,0,_app->getSize().width,_app->getSize().height};
     VkClearValue clearvalue[4];
     clearvalue[0].color        = {{0.0f, 1.0f, .0f, 1.0f}};
     clearvalue[1].depthStencil = {1.0f, 0};
     clearvalue[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
     clearvalue[3].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
     renderPassBeginInfo.clearValueCount = 4;
     renderPassBeginInfo.pClearValues = clearvalue;
     vkCmdBeginRenderPass(cmd,&renderPassBeginInfo,VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
     std::queue<SceneNode*> nodes;
     nodes.push(_app->_scene._root.get());
     auto       sceneVBuffers = _app->_modelLoader.getSceneVBuffers();
     auto       sceneIBuffers = _app->_modelLoader.getSceneIBuffers();
     auto       meshes        = _app->_modelLoader.getSceneMeshes();
     VkViewport viewport      = {
         0.0f, 0.0f, (float) _app->getSize().width, (float) _app->getSize().height,
         0.0f, 1.0f};
     VkRect2D scissor = {{0, 0}, _app->getSize()};
     vkCmdSetViewport(cmd, 0, 1, &viewport);
     vkCmdSetScissor(cmd, 0, 1, &scissor);
     vkCmdPushDescriptorSetKHR(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS, _razePipelineLayout, 0,
                           _writeDescriptorSets.size(), _writeDescriptorSets.data());
     VkExtent2D fragmentSize = {1, 1};
     VkFragmentShadingRateCombinerOpKHR combiner_ops[2];
     combiner_ops[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
	 combiner_ops[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
     vkCmdSetFragmentShadingRateKHR(cmd, &fragmentSize, combiner_ops);
                           {
         // ScopeTimer timer;
         while (!nodes.empty())
         {
             SceneNode* currnode = nodes.front();
             nodes.pop();
             if (!currnode->_meshIdx.empty())
             {
                 for (auto& meshIdx : currnode->_meshIdx)
                 {
                     Constants constants;
                     constants.model  = currnode->_transform;
                     constants.matIdx = meshes[meshIdx]._materialIndex;
                     // LOGI(std::to_string(constants.matIdx).c_str());
                     // vkCmdPushConstants(
                     //     cmd, _razePipelineLayout,
                     //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     //     sizeof(Constants), &constants);
                     VkDeviceSize offset = 0;
                     vkCmdBindVertexBuffers(cmd, 0, 1,
                                            &(sceneVBuffers[meshes[meshIdx]._vBufferIdx].buffer),
                                            &offset);
                     vkCmdBindIndexBuffer(cmd,
                     sceneIBuffers[meshes[meshIdx]._iBufferIdx].buffer,
                                          0, VkIndexType::VK_INDEX_TYPE_UINT32);
                     vkCmdDrawIndexed(cmd,
                                      _app->_modelLoader.getSceneMeshes()[meshIdx]._faceCnt
                                      * 3, 1, 0, 0, 0);
                 }
             }
             for (auto& child : currnode->_children)
             {
                 nodes.push(child.get());
             }
         }
     }
     vkCmdEndRenderPass(cmd);
}
    

void                 ShadingRateRenderer::SetScene(Scene* scene) {}
void                 ShadingRateRenderer::OnResize(int width, int height) {}
void                 ShadingRateRenderer::OnDestroy() {}
std::vector<VkDescriptorImageInfo> image_infos;
void                 ShadingRateRenderer::initPipeline()
 {
     nvvk::DescriptorSetBindings bindings;
     bindings.addBinding({0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT,nullptr});
     bindings.addBinding({1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT,nullptr});
     bindings.addBinding({2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
     bindings.addBinding({3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,uint32_t(_app->_modelLoader.getSceneTextures().size()),VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
     VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
     writeDescriptorSet.descriptorCount = 1;
     writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
     writeDescriptorSet.dstBinding = 0;
     writeDescriptorSet.pBufferInfo = &_renderUniformBuffer->descriptor;
     _writeDescriptorSets.push_back(writeDescriptorSet);
     writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
     writeDescriptorSet.descriptorCount = 1;
     writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
     writeDescriptorSet.dstBinding = 1;
     writeDescriptorSet.pImageInfo = &_gradientTexture->descriptor;
     _writeDescriptorSets.push_back(writeDescriptorSet);
     writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
     writeDescriptorSet.descriptorCount = 1;
     writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
     writeDescriptorSet.dstBinding = 2;
     writeDescriptorSet.pBufferInfo = &_app->_modelLoader.getMaterialBuffer()->descriptor;
     _writeDescriptorSets.push_back(writeDescriptorSet);
     writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
     writeDescriptorSet.descriptorCount = _app->_modelLoader.getSceneTextures().size();
     writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
     writeDescriptorSet.dstBinding = 3;
     image_infos.resize(_app->_modelLoader.getSceneTextures().size());
     for (int i = 0; i < _app->_modelLoader.getSceneTextures().size(); ++i)
     {
        image_infos[i] = _app->_modelLoader.getSceneTextures()[i]->descriptor;
     }
     writeDescriptorSet.pImageInfo = image_infos.data();
     _writeDescriptorSets.push_back(writeDescriptorSet);
     _shadingRateSetLayout = bindings.createLayout(_app->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT, nvvk::DescriptorSupport::INDEXING_EXT);
     VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
     pipelineLayoutInfo.setLayoutCount = 1;
     pipelineLayoutInfo.pSetLayouts    = &_shadingRateSetLayout;
     pipelineLayoutInfo.pushConstantRangeCount = 0;
     pipelineLayoutInfo.pPushConstantRanges    = nullptr;
     vkCreatePipelineLayout(_app->getDevice(), &pipelineLayoutInfo, nullptr, &_razePipelineLayout);
     nvvk::GraphicsPipelineGeneratorCombined gpipelineState(_app->getDevice(), _razePipelineLayout,
         _shadingRateRenderPass);
     gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
     gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
     gpipelineState.rasterizationState.frontFace       = VK_FRONT_FACE_COUNTER_CLOCKWISE;
     gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
     gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
     std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
         {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
         {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
         {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 24},
         {3, 0, VK_FORMAT_R32G32_SFLOAT, 36},

     };

     gpipelineState.addAttributeDescriptions(vertexInputAttributeDescriptions);
     VkVertexInputBindingDescription vertexInputBindingDescription = {0, sizeof(Vertex),
                                                                      VK_VERTEX_INPUT_RATE_VERTEX};
     gpipelineState.addBindingDescription(vertexInputBindingDescription);

     std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR,
                                                VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR};
     gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
     for (int i = 0; i < dynamicStates.size(); ++i)
     {
         gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
     }
     gpipelineState.addShader(nvh::loadFile("spv/vrs.vert.spv", true), VK_SHADER_STAGE_VERTEX_BIT,
                             "main");
     gpipelineState.addShader(nvh::loadFile("spv/vrs.frag.spv", true),
                              VK_SHADER_STAGE_FRAGMENT_BIT, "main");
     std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(2);
      gpipelineState.clearBlendAttachmentStates();
     for(auto& state:colorBlendAttachmentStates)
     {
        state.blendEnable = VK_FALSE;
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
         gpipelineState.addBlendAttachmentState(state);
     }

     _razePipeline = gpipelineState.createPipeline();

     _computePass = ComputePass(_app);
     _computePass.addInputTexture(_gradientTexture);
     _computePass.addComponent(_shadingRateTexture,VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR);
     _computePass.setShaderCode("spv/computeShadingRate.comp.spv");
     _computePass.build(_app);
}
void                 ShadingRateRenderer::createRenderResource() {
     _outputTexture = PlayApp::AllocTexture<Texture>();
     _shadingRateTexture = PlayApp::AllocTexture<Texture>();
     _gradientTexture = PlayApp::AllocTexture<Texture>();
     VkImageCreateInfo image2DCreateinfo = nvvk::makeImage2DCreateInfo(_app->getSize(),VK_FORMAT_R8G8B8A8_UNORM,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
     auto cmd = _app->createTempCmdBuffer();
     auto texture = _app->getAlloc().createTexture(cmd,0,nullptr,image2DCreateinfo,
                                                  nvvk::makeSamplerCreateInfo(),VK_IMAGE_LAYOUT_UNDEFINED);
     _outputTexture->image      = texture.image;
     _outputTexture->memHandle  = texture.memHandle;
     _outputTexture->descriptor = texture.descriptor;
     _outputTexture->_format    = image2DCreateinfo.format;
     CUSTOM_NAME_VK(_app->m_debug, _outputTexture->image);
     image2DCreateinfo.format = VK_FORMAT_R8_UINT;
     image2DCreateinfo.usage  = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_STORAGE_BIT;
     image2DCreateinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     texture = _app->getAlloc().createTexture(cmd,0,nullptr,image2DCreateinfo,
                                                  nvvk::makeSamplerCreateInfo(),VK_IMAGE_LAYOUT_UNDEFINED);
     _shadingRateTexture->image      = texture.image;
     _shadingRateTexture->memHandle  = texture.memHandle;
     _shadingRateTexture->descriptor = texture.descriptor;
     _shadingRateTexture->_format    = image2DCreateinfo.format;
     CUSTOM_NAME_VK(_app->m_debug, _shadingRateTexture->image);
     image2DCreateinfo.format = VK_FORMAT_R8G8_UINT;
     image2DCreateinfo.usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
     image2DCreateinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     texture = _app->getAlloc().createTexture(cmd,0,nullptr,image2DCreateinfo,
                                                  nvvk::makeSamplerCreateInfo(),VK_IMAGE_LAYOUT_GENERAL);
     _gradientTexture->image      = texture.image;
     _gradientTexture->memHandle  = texture.memHandle;
     _gradientTexture->descriptor = texture.descriptor;
     _gradientTexture->_format    = image2DCreateinfo.format;
     CUSTOM_NAME_VK(_app->m_debug, _gradientTexture->image);

     {
         VkImageCreateInfo depthImageCreateInfo = nvvk::makeImage2DCreateInfo(VkExtent2D{_app->getSize().width, _app->getSize().height}, VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);
         vkCreateImage(_app->getDevice(), &depthImageCreateInfo, nullptr, &_depthTexture.image);
         // Allocate the memory
         VkMemoryRequirements memReqs;
         vkGetImageMemoryRequirements(_app->getDevice(), _depthTexture.image, &memReqs);
         VkMemoryAllocateInfo memAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
         memAllocInfo.allocationSize = memReqs.size;
         memAllocInfo.memoryTypeIndex =
             _app->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
         vkAllocateMemory(_app->getDevice(), &memAllocInfo, nullptr, &_depthTexture.memory);
         // Bind image and memory
         vkBindImageMemory(_app->getDevice(), _depthTexture.image, _depthTexture.memory,0);
         VkImageSubresourceRange subresourceRange{};
         subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT ;
         subresourceRange.levelCount = 1;
         subresourceRange.layerCount = 1;

         VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
         imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
         imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
         imageMemoryBarrier.image                 = _depthTexture.image;
         imageMemoryBarrier.subresourceRange      = subresourceRange;
         imageMemoryBarrier.srcAccessMask         = VkAccessFlags();
         imageMemoryBarrier.dstAccessMask         = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
         const VkPipelineStageFlags srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
         const VkPipelineStageFlags destStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
         vkCmdPipelineBarrier(cmd, srcStageMask, destStageMask, VK_FALSE, 0, nullptr, 0, nullptr, 1,
                              &imageMemoryBarrier);
         VkImageViewCreateInfo imageViewCreateInfo = nvvk::makeImage2DViewCreateInfo(_depthTexture.image, VK_FORMAT_D32_SFLOAT,
                                                                  VK_IMAGE_ASPECT_DEPTH_BIT);
         vkCreateImageView(_app->getDevice(), &imageViewCreateInfo, nullptr, &_depthTexture.descriptor.imageView);
     }

     _app->submitTempCmdBuffer(cmd);

     _renderUniformBuffer = PlayApp::AllocBuffer<Buffer>();
     VkBufferCreateInfo bufferInfo = nvvk::makeBufferCreateInfo(sizeof(_ShaderRateUniformStruct), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
     auto nvvkBuffer = _app->_alloc.createBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
     _renderUniformBuffer->buffer            = nvvkBuffer.buffer;
     _renderUniformBuffer->address           = nvvkBuffer.address;
     _renderUniformBuffer->memHandle         = nvvkBuffer.memHandle;
     _renderUniformBuffer->descriptor = {nvvkBuffer.buffer,0,sizeof(_ShaderRateUniformStruct)};
     CUSTOM_NAME_VK(_app->m_debug, _renderUniformBuffer->buffer);
}

void                 ShadingRateRenderer::createRenderPass()
{
     _physical_device_fragment_shading_rate_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
     VkPhysicalDeviceProperties2KHR deviceProperties{};
     deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
     deviceProperties.pNext = &_physical_device_fragment_shading_rate_properties;
     vkGetPhysicalDeviceProperties2(this->_app->getPhysicalDevice(), &deviceProperties);

     std::vector<VkAttachmentDescription2KHR> attachments(4,{VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2});
// Color attachment
     attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
     attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
     attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
     attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
     attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
     attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
// Depth attachment
     attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
     attachments[1].format = VK_FORMAT_D32_SFLOAT;
     attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
     attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
     attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
// FragShading rate attachment
     attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
     attachments[2].format = VK_FORMAT_R8_UINT;
     attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
     attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
     attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     attachments[2].finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
// Frequency Fragment attachment
     attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
     attachments[3].format = VK_FORMAT_R8G8_UINT;
     attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
     attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
     attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
     attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
     attachments[3].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

     VkAttachmentReference2KHR depthReference = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
     depthReference.attachment = 1;
     depthReference.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
     depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
     VkAttachmentReference2 fragmentShadingReference={VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
     fragmentShadingReference.attachment = 2;
     fragmentShadingReference.layout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;

     VkFragmentShadingRateAttachmentInfoKHR fragmentShadingRateAttachInfo{VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR};
     fragmentShadingRateAttachInfo.pFragmentShadingRateAttachment = &fragmentShadingReference;
     fragmentShadingRateAttachInfo.shadingRateAttachmentTexelSize.width = _physical_device_fragment_shading_rate_properties.maxFragmentShadingRateAttachmentTexelSize.width;
     fragmentShadingRateAttachInfo.shadingRateAttachmentTexelSize.height = _physical_device_fragment_shading_rate_properties.maxFragmentShadingRateAttachmentTexelSize.height;

     VkAttachmentReference2 colorReference{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
     colorReference.attachment = 0;
     colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
     colorReference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
     VkAttachmentReference2 frequencyAttachRef {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
     frequencyAttachRef.attachment = 3;
     frequencyAttachRef.layout = VK_IMAGE_LAYOUT_GENERAL;
     frequencyAttachRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

     std::vector<VkAttachmentReference2> colorReferences = {colorReference};
     colorReferences.push_back(frequencyAttachRef);

     VkSubpassDescription2 subpassDescription{VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2};
     subpassDescription.colorAttachmentCount = colorReferences.size();
     subpassDescription.pColorAttachments = colorReferences.data();
     subpassDescription.inputAttachmentCount = 0;
     subpassDescription.pInputAttachments  = nullptr;
     subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
     subpassDescription.pDepthStencilAttachment = &depthReference;
     subpassDescription.pPreserveAttachments = nullptr;
     subpassDescription.pResolveAttachments = nullptr;
     subpassDescription.pNext = &fragmentShadingRateAttachInfo;

     std::vector<VkSubpassDependency2> dependency2s(2,{VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2});
     dependency2s[0].srcSubpass = VK_SUBPASS_EXTERNAL;
     dependency2s[0].dstSubpass = 0;
     dependency2s[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
     dependency2s[0].viewOffset = 0;
     dependency2s[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
     dependency2s[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
     dependency2s[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
     dependency2s[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

     dependency2s[1].srcSubpass = 0;
     dependency2s[1].dstSubpass = VK_SUBPASS_EXTERNAL;
     dependency2s[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
     dependency2s[1].viewOffset = 0;
     dependency2s[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
     dependency2s[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
     dependency2s[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
     dependency2s[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

     VkRenderPassCreateInfo2 renderPassCreateInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2};
     renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
     renderPassCreateInfo.pAttachments = attachments.data();
     renderPassCreateInfo.dependencyCount = static_cast<uint32_t> (dependency2s.size());
     renderPassCreateInfo.pDependencies = dependency2s.data();
     renderPassCreateInfo.subpassCount = 1;
     renderPassCreateInfo.pSubpasses = &subpassDescription;
     renderPassCreateInfo.correlatedViewMaskCount = 0;
     renderPassCreateInfo.pNext = nullptr;
     vkCreateRenderPass2(_app->getDevice(),&renderPassCreateInfo,nullptr,&_shadingRateRenderPass);
     CUSTOM_NAME_VK(_app->m_debug,_shadingRateRenderPass);
}

void                 ShadingRateRenderer::createFrameBuffer() {
     std::vector<VkImageView> imageViews(4);
     imageViews[0] = _outputTexture->descriptor.imageView;
     imageViews[1] = _depthTexture.descriptor.imageView;
     imageViews[2] = _shadingRateTexture->descriptor.imageView;
     imageViews[3] = _gradientTexture->descriptor.imageView;
     VkFramebufferCreateInfo fboInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
     fboInfo.width = _app->getSize().width;
     fboInfo.height = _app->getSize().height;
     fboInfo.layers = 1;
     fboInfo.renderPass = _shadingRateRenderPass;
     fboInfo.attachmentCount= 4;
     fboInfo.pAttachments = imageViews.data();
     vkCreateFramebuffer(_app->getDevice(),&fboInfo,nullptr,&_shadingRateFramebuffer);
     CUSTOM_NAME_VK(_app->m_debug,_shadingRateFramebuffer);



}
} // namespace Play