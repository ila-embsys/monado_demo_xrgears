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


#include "vulkan_texture.h"
#include "vulkan_framebuffer.hpp"

#include "vulkan_pipeline.hpp"

class pipeline_equirect : public vulkan_pipeline
{
public:
  VkDescriptorSet descriptor_sets[2];

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
  update_vp(glm::mat4 projection, glm::mat4 view, uint32_t eye);

  void
  draw(VkCommandBuffer cmd_buffer, uint32_t eye);
};
