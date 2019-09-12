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

class pipeline_gears
{
public:
  VkDevice device;
  VkPipeline pipeline;
  VkDescriptorPool descriptor_pool;

  std::vector<Gear *> nodes;

  struct UBOLights
  {
    glm::vec4 lights[4];
  } ubo_lights;

  struct
  {
    vulkan_buffer lights;
  } uniform_buffers;

  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;

  pipeline_gears(vulkan_device *vulkan_device,
                 VkRenderPass render_pass,
                 VkPipelineCache pipeline_cache,
                 VkDescriptorBufferInfo *camera_descriptor[2]);
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
  update_uniform_buffers(float animation_timer);

  void
  init_uniform_buffers(vulkan_device *vk_device);
};
