/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "glm_inc.hpp"

#include "vulkan_texture.hpp"
#include "vulkan_framebuffer.hpp"

class pipeline_equirect
{
public:
  VkDevice device;
  VkPipeline pipeline;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSet descriptor_sets[2];
  VkDescriptorPool descriptor_pool;

  vulkan_texture texture;

  struct
  {
    vulkan_buffer views[2];
  } uniform_buffers;

  struct UBOView
  {
    glm::mat4 vp;
  } ubo_views[2];

  pipeline_equirect(vulkan_device *vulkan_device,
                    VkQueue queue,
                    VkRenderPass render_pass,
                    VkPipelineCache pipeline_cache);

  ~pipeline_equirect();

  void
  init_texture(vulkan_device *vk_device, VkQueue queue);

  void
  init_descriptor_pool();

  void
  init_descriptor_set_layouts();

  void
  init_descriptor_sets(uint32_t eye);

  void
  init_pipeline(VkRenderPass render_pass, VkPipelineCache pipeline_cache);

  void
  init_uniform_buffers(vulkan_device *vk_device);

  void
  update_uniform_buffers(glm::mat4 projection, glm::mat4 view, uint32_t eye);

  void
  draw(VkCommandBuffer cmd_buffer, uint32_t eye);
};
