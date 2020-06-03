/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include "vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  VkDevice device;
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorBufferInfo descriptor;
  VkDeviceSize size;
  VkDeviceSize alignment;
  void *mapped;

  VkBufferUsageFlags usage_flags;
  VkMemoryPropertyFlags memory_property_flags;
} vulkan_buffer;

VkResult
vulkan_buffer_map(vulkan_buffer *self);

void
vulkan_buffer_unmap(vulkan_buffer *self);

VkResult
vulkan_buffer_bind(vulkan_buffer *self);

void
vulkan_buffer_setup_descriptor(vulkan_buffer *self);

void
vulkan_buffer_destroy(vulkan_buffer *self);

#ifdef __cplusplus
}
#endif
