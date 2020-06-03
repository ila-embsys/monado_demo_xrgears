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

#include <assert.h>
#include <vulkan/vulkan.h>

#include <exception>
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>

#include "vulkan_buffer.h"
#include "log.h"

class vulkan_device
{
public:
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceFeatures features;
  VkPhysicalDeviceMemoryProperties memory_properties;
  std::vector<VkQueueFamilyProperties> queue_family_properties;
  std::vector<std::string> supported_extensions;

  VkCommandPool cmd_pool = VK_NULL_HANDLE;

  bool enable_debug_markers = false;

  struct
  {
    uint32_t graphics;
  } queue_family_indices;

  /**  @brief Typecast to VkDevice */
  operator VkDevice()
  {
    return device;
  }

  explicit vulkan_device(VkPhysicalDevice physical_device)
  {
    assert(physical_device);
    this->physical_device = physical_device;

    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vkGetPhysicalDeviceFeatures(physical_device, &features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount,
                                             nullptr);
    assert(queueFamilyCount > 0);
    queue_family_properties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount,
                                             queue_family_properties.data());

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extCount,
                                         nullptr);
    if (extCount > 0) {
      std::vector<VkExtensionProperties> extensions(extCount);
      if (vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, &extCount, &extensions.front()) ==
          VK_SUCCESS)
        for (auto ext : extensions)
          supported_extensions.push_back(ext.extensionName);
    }
  }

  ~vulkan_device()
  {
    if (cmd_pool)
      vkDestroyCommandPool(device, cmd_pool, nullptr);
    if (device)
      vkDestroyDevice(device, nullptr);
  }

  uint32_t
  get_memory_type(uint32_t typeBits,
                  VkMemoryPropertyFlags properties,
                  VkBool32 *memTypeFound = nullptr)
  {
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1) {
        if ((memory_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
          if (memTypeFound)
            *memTypeFound = true;
          return i;
        }
      }
      typeBits >>= 1;
    }

    if (memTypeFound) {
      *memTypeFound = false;
      return 0;
    } else {
      throw std::runtime_error("Could not find a matching memory type");
    }
  }

  uint32_t
  get_graphics_queue_index()
  {
    for (uint32_t i = 0;
         i < static_cast<uint32_t>(queue_family_properties.size()); i++) {
      if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        return i;
      }
    }

    throw std::runtime_error("Could not find a matching queue family index");
  }

  VkResult
  create_device(VkPhysicalDeviceFeatures enabledFeatures)
  {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

    const float defaultQueuePriority(0.0f);

    queue_family_indices.graphics = get_graphics_queue_index();
    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = queue_family_indices.graphics,
      .queueCount = 1,
      .pQueuePriorities = &defaultQueuePriority
    };

    queueCreateInfos.push_back(queueInfo);

    VkDeviceCreateInfo deviceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .pEnabledFeatures = &enabledFeatures
    };

    VkResult result =
      vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device);

    if (result == VK_SUCCESS)
      // Create a default command pool for graphics command buffers
      cmd_pool = create_cmd_pool(queue_family_indices.graphics);

    return result;
  }

  VkResult
  create_buffer(VkBufferUsageFlags usageFlags,
                VkMemoryPropertyFlags memoryPropertyFlags,
                VkDeviceSize size,
                VkBuffer *buffer,
                VkDeviceMemory *memory,
                void *data = nullptr)
  {
    // Create the buffer handle
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vk_check(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Find a memory type index that fits the properties of the buffer
      .memoryTypeIndex =
        get_memory_type(memReqs.memoryTypeBits, memoryPropertyFlags)
    };

    vk_check(vkAllocateMemory(device, &memAlloc, nullptr, memory));

    // If a pointer to the buffer data has been passed, map the buffer and copy
    // over the data
    if (data != nullptr) {
      void *mapped;
      vk_check(vkMapMemory(device, *memory, 0, size, 0, &mapped));
      memcpy(mapped, data, size);
      // If host coherency hasn't been requested, do a manual flush to make
      // writes visible
      if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {

        VkMappedMemoryRange mappedRange = {
          .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          .memory = *memory,
          .offset = 0,
          .size = size
        };

        vkFlushMappedMemoryRanges(device, 1, &mappedRange);
      }
      vkUnmapMemory(device, *memory);
    }

    // Attach the memory to the buffer object
    vk_check(vkBindBufferMemory(device, *buffer, *memory, 0));

    return VK_SUCCESS;
  }

  void
  create_and_map(vulkan_buffer *buffer, VkDeviceSize size)
  {
    vk_check(create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           buffer, size));

    // Map persistent
    vk_check(vulkan_buffer_map(buffer));
  }

  VkResult
  create_buffer(VkBufferUsageFlags usageFlags,
                VkMemoryPropertyFlags memoryPropertyFlags,
                vulkan_buffer *buffer,
                VkDeviceSize size,
                void *data = nullptr)
  {
    buffer->device = device;

    // Create the buffer handle
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usageFlags
    };
    vk_check(
      vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer->buffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Find a memory type index that fits the properties of the buffer
      .memoryTypeIndex =
        get_memory_type(memReqs.memoryTypeBits, memoryPropertyFlags)
    };

    vk_check(vkAllocateMemory(device, &memAlloc, nullptr, &buffer->memory));

    buffer->alignment = memReqs.alignment;
    buffer->size = memAlloc.allocationSize;
    buffer->usage_flags = usageFlags;
    buffer->memory_property_flags = memoryPropertyFlags;

    // If a pointer to the buffer data has been passed, map the buffer and copy
    // over the data
    if (data != nullptr) {
      vk_check(vulkan_buffer_map(buffer));
      memcpy(buffer->mapped, data, size);
      vulkan_buffer_unmap(buffer);
    }

    // Initialize a default descriptor that covers the whole buffer size
    vulkan_buffer_setup_descriptor(buffer);

    // Attach the memory to the buffer object
    return vulkan_buffer_bind(buffer);
  }

  VkCommandPool
  create_cmd_pool(uint32_t queueFamilyIndex,
                  VkCommandPoolCreateFlags createFlags =
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
  {
    VkCommandPoolCreateInfo cmdPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = createFlags,
      .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool cmdPool;
    vk_check(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
    return cmdPool;
  }

  VkCommandBuffer
  create_cmd_buffer(VkCommandBufferLevel level, bool begin = false)
  {
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = level,
      .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    vk_check(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start recording for the new command buffer
    if (begin) {
      VkCommandBufferBeginInfo cmdBufInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
      };
      vk_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
  }

  void
  flush_cmd_buffer(VkCommandBuffer commandBuffer,
                   VkQueue queue,
                   bool free = true)
  {
    if (commandBuffer == VK_NULL_HANDLE)
      return;

    vk_check(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &commandBuffer };

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo = { .sType =
                                      VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    vk_check(vkCreateFence(device, &fenceInfo, nullptr, &fence));

    // Submit to the queue
    vk_check(vkQueueSubmit(queue, 1, &submitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    vk_check(vkWaitForFences(device, 1, &fence, VK_TRUE, INT64_MAX));

    vkDestroyFence(device, fence, nullptr);

    if (free)
      vkFreeCommandBuffers(device, cmd_pool, 1, &commandBuffer);
  }

  bool
  enable_if_supported(std::vector<const char *> *extensions, const char *name)
  {
    if (is_extension_supported(name)) {
      xrg_log_d("device: Enabling supported %s.", name);
      extensions->push_back(name);
      return true;
    } else {
      xrg_log_w("device: %s not supported.", name);
      return false;
    }
  }

  bool
  is_extension_supported(std::string extension)
  {
    return std::find(supported_extensions.begin(), supported_extensions.end(),
                     extension) != supported_extensions.end();
  }
};
