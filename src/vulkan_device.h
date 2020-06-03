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

#include <assert.h>
#include <vulkan/vulkan.h>

#include "vulkan_buffer.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceFeatures features;
  VkPhysicalDeviceMemoryProperties memory_properties;
  VkQueueFamilyProperties *queue_family_properties;
  uint32_t queue_family_count;

  VkCommandPool cmd_pool;

  uint32_t graphics_family_index;
} vulkan_device;

vulkan_device *
vulkan_device_create(VkPhysicalDevice physical_device);

void
vulkan_device_destroy(vulkan_device *self);

bool
vulkan_device_get_memory_type(vulkan_device *self,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags properties,
                              uint32_t *out_index);

VkResult
vulkan_device_create_device(vulkan_device *self);

VkResult
vulkan_device_create_buffer(vulkan_device *self,
                            vulkan_buffer *buffer,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memory_flags,
                            VkDeviceSize size,
                            void *data);

void
vulkan_device_create_and_map(vulkan_device *self,
                             vulkan_buffer *buffer,
                             VkDeviceSize size);

VkCommandBuffer
vulkan_device_create_cmd_buffer(vulkan_device *self);

void
vulkan_device_flush_cmd_buffer(vulkan_device *self,
                               VkCommandBuffer commandBuffer,
                               VkQueue queue);

#ifdef __cplusplus
}
#endif
