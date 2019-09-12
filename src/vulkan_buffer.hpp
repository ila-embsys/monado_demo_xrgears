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

#include <vector>
#include <cassert>
#include <cstring>
#include "vulkan/vulkan.h"


struct vulkan_buffer
{
  VkDevice device;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDescriptorBufferInfo descriptor;
  VkDeviceSize size = 0;
  VkDeviceSize alignment = 0;
  void* mapped = nullptr;

  /** @brief Usage flags to be filled by external source at buffer creation (to
   * query at some later point) */
  VkBufferUsageFlags usageFlags;
  /** @brief Memory propertys flags to be filled by external source at buffer
   * creation (to query at some later point) */
  VkMemoryPropertyFlags memoryPropertyFlags;

  /**
   * Map a memory range of this buffer. If successful, mapped points to the
   * specified buffer range.
   *
   * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE
   * to map the complete buffer range.
   * @param offset (Optional) Byte offset from beginning
   *
   * @return VkResult of the buffer mapping call
   */
  VkResult
  map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
  {
    return vkMapMemory(device, memory, offset, size, 0, &mapped);
  }

  /**
   * Unmap a mapped memory range
   *
   * @note Does not return a result as vkUnmapMemory can't fail
   */
  void
  unmap()
  {
    if (mapped) {
      vkUnmapMemory(device, memory);
      mapped = nullptr;
    }
  }

  /**
   * Attach the allocated memory block to the buffer
   *
   * @param offset (Optional) Byte offset (from the beginning) for the memory
   * region to bind
   *
   * @return VkResult of the bindBufferMemory call
   */
  VkResult
  bind(VkDeviceSize offset = 0)
  {
    return vkBindBufferMemory(device, buffer, memory, offset);
  }

  /**
   * Setup the default descriptor for this buffer
   *
   * @param size (Optional) Size of the memory range of the descriptor
   * @param offset (Optional) Byte offset from beginning
   *
   */
  void
  setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
  {
    descriptor.offset = offset;
    descriptor.buffer = buffer;
    descriptor.range = size;
  }
  /**
   * Release all Vulkan resources held by this buffer
   */
  void
  destroy()
  {
    if (buffer)
      vkDestroyBuffer(device, buffer, nullptr);
    if (memory)
      vkFreeMemory(device, memory, nullptr);
  }
};
