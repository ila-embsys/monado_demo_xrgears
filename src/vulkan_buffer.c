/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_buffer.h"

VkResult
vulkan_buffer_map(vulkan_buffer *self)
{
  VkDeviceSize size = VK_WHOLE_SIZE;
  VkDeviceSize offset = 0;
  return vkMapMemory(self->device, self->memory, offset, size, 0,
                     &self->mapped);
}

void
vulkan_buffer_unmap(vulkan_buffer *self)
{
  if (self->mapped) {
    vkUnmapMemory(self->device, self->memory);
    self->mapped = NULL;
  }
}

VkResult
vulkan_buffer_bind(vulkan_buffer *self)
{
  return vkBindBufferMemory(self->device, self->buffer, self->memory, 0);
}

void
vulkan_buffer_setup_descriptor(vulkan_buffer *self)
{
  self->descriptor = (VkDescriptorBufferInfo){
    .offset = 0,
    .buffer = self->buffer,
    .range = VK_WHOLE_SIZE,
  };
}

void
vulkan_buffer_destroy(vulkan_buffer *self)
{
  if (self->buffer)
    vkDestroyBuffer(self->device, self->buffer, NULL);
  if (self->memory)
    vkFreeMemory(self->device, self->memory, NULL);
}
