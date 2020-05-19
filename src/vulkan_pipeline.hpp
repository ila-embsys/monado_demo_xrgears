/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>

class vulkan_pipeline
{
public:
  VkDevice device;
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;

  VkDescriptorPool descriptor_pool;
  VkDescriptorSetLayout descriptor_set_layout;

  virtual ~vulkan_pipeline() {}

  virtual void
  draw(VkCommandBuffer cmd_buffer, uint32_t eye) = 0;
};
