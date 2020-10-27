/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "pipeline_equirect.hpp"

#include "log.h"
#include "vulkan_shader.h"

#include "sky_plane_equirect.frag.h"
#include "sky_plane_equirect.vert.h"

#include "textures.h"

#include <array>
#include <vector>

pipeline_equirect::pipeline_equirect(vulkan_device *vulkan_device,
                                     VkQueue queue,
                                     VkRenderPass render_pass,
                                     VkPipelineCache pipeline_cache)
{
  this->device = vulkan_device->device;
  init_texture(vulkan_device, queue);
  init_uniform_buffers(vulkan_device);
  init_descriptor_set_layouts();
  init_pipeline(render_pass, pipeline_cache);
  init_descriptor_pool();
  for (uint32_t i = 0; i < 2; i++)
    init_descriptor_sets(i);
}

pipeline_equirect::~pipeline_equirect()
{
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
  vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
  for (uint32_t i = 0; i < 2; i++)
    vulkan_buffer_destroy(&uniform_buffers.views[i]);
  vulkan_texture_destroy(&texture);
}

void
pipeline_equirect::init_texture(vulkan_device *vk_device, VkQueue queue)
{
  vulkan_texture_load_ktx(&texture, rooftop_bytes(), rooftop_size(), vk_device,
                          queue, VK_FORMAT_BC2_SRGB_BLOCK,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void
pipeline_equirect::draw(VkCommandBuffer cmd_buffer, uint32_t eye)
{
  // Skysphere
  vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &descriptor_sets[eye], 0,
                          NULL);

  /* Draw 3 verts from which we construct the fullscreen quad in
   * the shader*/
  vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
}

void
pipeline_equirect::init_descriptor_pool()
{
  std::vector<VkDescriptorPoolSize> poolSizes = {
    { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 3 },
    { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 3 }
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 2,
    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes = poolSizes.data()
  };

  vk_check(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr,
                                  &descriptor_pool));
}

void
pipeline_equirect::init_descriptor_set_layouts()
{
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
    // Binding 0 : Fragment shader ubo
    { .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
    // Binding 1 : Skydome texture
    { .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
  };

  VkDescriptorSetLayoutCreateInfo descriptorLayout = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
    .pBindings = setLayoutBindings.data()
  };
  vk_check(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr,
                                       &descriptor_set_layout));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descriptor_set_layout
  };
  vk_check(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
                                  &pipeline_layout));
}

void
pipeline_equirect::init_descriptor_sets(uint32_t eye)
{
  VkDescriptorSetAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &descriptor_set_layout
  };
  vk_check(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_sets[eye]));

  VkDescriptorImageInfo descriptor = vulkan_texture_get_descriptor(&texture);

  std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    // Binding 0 : Vertex shader ubo
    (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = descriptor_sets[eye],
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo =
                              &uniform_buffers.views[eye].descriptor },
    // Binding 1 : Fragment shader color map
    (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .dstSet = descriptor_sets[eye],
                            .dstBinding = 1,
                            .descriptorCount = 1,
                            .descriptorType =
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &descriptor },
  };

  vkUpdateDescriptorSets(device,
                         static_cast<uint32_t>(writeDescriptorSets.size()),
                         writeDescriptorSets.data(), 0, nullptr);
}

void
pipeline_equirect::init_pipeline(VkRenderPass render_pass,
                                 VkPipelineCache pipeline_cache)
{
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };

  VkPipelineRasterizationStateCreateInfo rasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.0f
  };

  VkPipelineColorBlendAttachmentState blendAttachmentState = {
    .blendEnable = VK_FALSE, .colorWriteMask = 0xf
  };

  VkPipelineColorBlendStateCreateInfo colorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &blendAttachmentState
  };

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_FALSE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .front = { .compareOp = VK_COMPARE_OP_ALWAYS },
    .back = { .compareOp = VK_COMPARE_OP_ALWAYS }
  };

  VkPipelineViewportStateCreateInfo viewportState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1
  };

  VkPipelineMultisampleStateCreateInfo multisampleState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
  };

  std::vector<VkDynamicState> dynamicStateEnables = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
    .pDynamicStates = dynamicStateEnables.data()
  };

  VkPipelineVertexInputStateCreateInfo vertexInputState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
    vulkan_shader_load(device, sky_plane_equirect_vert,
                       sizeof(sky_plane_equirect_vert),
                       VK_SHADER_STAGE_VERTEX_BIT),
    vulkan_shader_load(device, sky_plane_equirect_frag,
                       sizeof(sky_plane_equirect_frag),
                       VK_SHADER_STAGE_FRAGMENT_BIT)
  };

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = static_cast<uint32_t>(shaderStages.size()),
    .pStages = shaderStages.data(),
    .pVertexInputState = &vertexInputState,
    .pInputAssemblyState = &inputAssemblyState,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizationState,
    .pMultisampleState = &multisampleState,
    .pDepthStencilState = &depthStencilState,
    .pColorBlendState = &colorBlendState,
    .pDynamicState = &dynamicState,
    .layout = pipeline_layout,
    .renderPass = render_pass,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1
  };

  vk_check(vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                     &pipelineCreateInfo, nullptr, &pipeline));

  vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
}

// Prepare and initialize uniform buffer containing shader uniforms
void
pipeline_equirect::init_uniform_buffers(vulkan_device *vk_device)
{
  // Skysphere vertex shader uniform buffer for each eye
  for (uint32_t i = 0; i < 2; i++)
    vulkan_device_create_and_map(vk_device, &uniform_buffers.views[i],
                                 sizeof(ubo_views[i]));
}

void
pipeline_equirect::update_vp(glm::mat4 projection, glm::mat4 view, uint32_t eye)
{
  ubo_views[eye].vp = glm::inverse(projection * glm::mat4(glm::mat3(view)));
  memcpy(uniform_buffers.views[eye].mapped, &ubo_views[eye],
         sizeof(ubo_views[eye]));
}
