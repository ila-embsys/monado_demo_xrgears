/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_device.h"

vulkan_device *
vulkan_device_create(VkPhysicalDevice physical_device)
{
  vulkan_device *self = malloc(sizeof(vulkan_device));

  assert(physical_device);
  self->physical_device = physical_device;

  vkGetPhysicalDeviceProperties(physical_device, &self->properties);
  vkGetPhysicalDeviceFeatures(physical_device, &self->features);
  vkGetPhysicalDeviceMemoryProperties(physical_device,
                                      &self->memory_properties);

  vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                           &self->queue_family_count, NULL);
  assert(self->queue_family_count > 0);
  self->queue_family_properties =
    malloc(sizeof(VkQueueFamilyProperties) * self->queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device, &self->queue_family_count, self->queue_family_properties);

  return self;
}

void
vulkan_device_destroy(vulkan_device *self)
{
  if (self->cmd_pool)
    vkDestroyCommandPool(self->device, self->cmd_pool, NULL);
  if (self->device)
    vkDestroyDevice(self->device, NULL);
  free(self);
}

bool
vulkan_device_get_memory_type(vulkan_device *self,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags properties,
                              uint32_t *out_index)
{
  for (uint32_t i = 0; i < self->memory_properties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      if ((self->memory_properties.memoryTypes[i].propertyFlags & properties) ==
          properties) {
        *out_index = i;
        return true;
      }
    }
    typeBits >>= 1;
  }

  *out_index = 0;
  return false;
}

static bool
_get_graphics_queue_index(vulkan_device *self)
{
  for (uint32_t i = 0; i < self->queue_family_count; i++) {
    if (self->queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      self->graphics_family_index = i;
      return true;
    }
  }

  self->graphics_family_index = 0;
  return false;
}

static VkCommandPool
_create_cmd_pool(vulkan_device *self)
{
  VkCommandPoolCreateFlags createFlags =
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandPoolCreateInfo cmdPoolInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = createFlags,
    .queueFamilyIndex = self->graphics_family_index,
  };

  VkCommandPool cmdPool;
  vk_check(vkCreateCommandPool(self->device, &cmdPoolInfo, NULL, &cmdPool));
  return cmdPool;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

VkResult
vulkan_device_create_device(vulkan_device *self)
{
  if (!_get_graphics_queue_index(self))
    xrg_log_e("Could not find graphics queue.");

  VkDeviceQueueCreateInfo queue_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = self->graphics_family_index,
    .queueCount = 1,
    .pQueuePriorities = (float[]){ 0.0f },
  };

  char *enabled_extensions[] = {
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
  };

  VkPhysicalDeviceFeatures enabled_features = {
    .samplerAnisotropy = VK_TRUE,
    .textureCompressionBC = VK_TRUE,
  };

  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
    .pEnabledFeatures = &enabled_features,
    .enabledExtensionCount = ARRAY_SIZE(enabled_extensions),
    .ppEnabledExtensionNames = (const char *const *)enabled_extensions,

  };

  VkResult result =
    vkCreateDevice(self->physical_device, &device_info, NULL, &self->device);

  if (result != VK_SUCCESS) {
    xrg_log_e("Could not create device.");
    return result;
  }

  self->cmd_pool = _create_cmd_pool(self);

  return result;
}

void
vulkan_device_create_and_map(vulkan_device *self,
                             vulkan_buffer *buffer,
                             VkDeviceSize size)
{
  VkMemoryPropertyFlags memory_flags =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vk_check(vulkan_device_create_buffer(self, buffer,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       memory_flags, size, NULL));

  // Map persistent
  vk_check(vulkan_buffer_map(buffer));
}

VkResult
vulkan_device_create_buffer(vulkan_device *self,
                            vulkan_buffer *buffer,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memory_flags,
                            VkDeviceSize size,
                            void *data)
{
  buffer->device = self->device;

  // Create the buffer handle
  VkBufferCreateInfo bufferCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
  };
  vk_check(
    vkCreateBuffer(self->device, &bufferCreateInfo, NULL, &buffer->buffer));

  // Create the memory backing up the buffer handle
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(self->device, buffer->buffer, &memReqs);

  VkMemoryAllocateInfo memAlloc = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = memReqs.size,
  };

  // Find a memory type index that fits the properties of the buffer
  if (!vulkan_device_get_memory_type(self, memReqs.memoryTypeBits, memory_flags,
                                     &memAlloc.memoryTypeIndex))
    xrg_log_e("Could not find memory type.");

  vk_check(vkAllocateMemory(self->device, &memAlloc, NULL, &buffer->memory));

  buffer->alignment = memReqs.alignment;
  buffer->size = memAlloc.allocationSize;
  buffer->usage_flags = usage;
  buffer->memory_property_flags = memory_flags;

  // If a pointer to the buffer data has been passed, map the buffer and copy
  // over the data
  if (data != NULL) {
    vk_check(vulkan_buffer_map(buffer));
    memcpy(buffer->mapped, data, size);
    vulkan_buffer_unmap(buffer);
  }

  // Initialize a default descriptor that covers the whole buffer size
  vulkan_buffer_setup_descriptor(buffer);

  // Attach the memory to the buffer object
  return vulkan_buffer_bind(buffer);
}

VkCommandBuffer
vulkan_device_create_cmd_buffer(vulkan_device *self)
{
  VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = self->cmd_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
  };

  VkCommandBuffer cmdBuffer;
  vk_check(
    vkAllocateCommandBuffers(self->device, &cmdBufAllocateInfo, &cmdBuffer));

  VkCommandBufferBeginInfo cmdBufInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };
  vk_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

  return cmdBuffer;
}

void
vulkan_device_flush_cmd_buffer(vulkan_device *self,
                               VkCommandBuffer commandBuffer,
                               VkQueue queue)
{
  if (commandBuffer == VK_NULL_HANDLE)
    return;

  vk_check(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
  };

  // Create fence to ensure that the command buffer has finished executing
  VkFenceCreateInfo fenceInfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };
  VkFence fence;
  vk_check(vkCreateFence(self->device, &fenceInfo, NULL, &fence));

  // Submit to the queue
  vk_check(vkQueueSubmit(queue, 1, &submitInfo, fence));
  // Wait for the fence to signal that command buffer has finished executing
  vk_check(vkWaitForFences(self->device, 1, &fence, VK_TRUE, INT64_MAX));

  vkDestroyFence(self->device, fence, NULL);

  vkFreeCommandBuffers(self->device, self->cmd_pool, 1, &commandBuffer);
}
