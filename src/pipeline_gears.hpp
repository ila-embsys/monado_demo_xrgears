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

#include <vulkan/vulkan.h>

#include "gear.hpp"

#include "vulkan_pipeline.hpp"

class pipeline_gears : public vulkan_pipeline
{
public:
  std::vector<Gear *> nodes;

  struct UBOLights
  {
    glm::vec4 lights[4];
  } ubo_lights;

  struct
  {
    glm::mat4 vp;
    glm::vec4 position;
  } ubo_camera[2];

  struct
  {
    vulkan_buffer lights;
    vulkan_buffer camera[2];
  } uniform_buffers;

  pipeline_gears(vulkan_device *vulkan_device,
                 VkRenderPass render_pass,
                 VkPipelineCache pipeline_cache);
  ~pipeline_gears();

  void
  draw(VkCommandBuffer command_buffer, uint32_t eye);

  void
  init_gears(vulkan_device *vk_device);

  void
  init_descriptor_pool();

  void
  init_descriptor_set_layout();

  void
  init_descriptor_sets(uint32_t eye, VkDescriptorBufferInfo *camera_descriptor);

  void
  init_pipeline(VkRenderPass render_pass, VkPipelineCache pipeline_cache);

  void
  update_lights();

  void
  update_time(float animation_timer);

  void
  update_vp(glm::mat4 projection,
            glm::mat4 view,
            glm::vec4 position,
            uint32_t eye);

  void
  init_uniform_buffers(vulkan_device *vk_device);
};
