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

#include <stdlib.h>

#include <vulkan/vulkan.h>

#include <string>
#include <fstream>
#include <vector>

#include "vulkan_device.hpp"
#include "vulkan_buffer.hpp"
#include "log.h"
#include "ktx_texture.h"

static void
set_image_layout(VkCommandBuffer cmd_buffer,
                 VkImage image,
                 VkImageLayout old_layout,
                 VkImageLayout new_layout,
                 VkImageSubresourceRange subresource_range,
                 VkAccessFlags src_access_mask,
                 VkAccessFlags dst_access_mask)
{
  VkImageMemoryBarrier imageMemoryBarrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = src_access_mask,
    .dstAccessMask = dst_access_mask,
    .oldLayout = old_layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = subresource_range,
  };

  vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);
}

class vulkan_texture
{
public:
  vulkan_device *device;

  VkImage image;
  VkImageLayout image_layout;
  VkDeviceMemory device_memory = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;

  uint32_t width, height;
  uint32_t mip_levels;
  uint32_t layer_count;

  bool created_from_image = false;

  VkDescriptorImageInfo
  get_descriptor()
  {
    VkDescriptorImageInfo descriptor = {
      .sampler = sampler,
      .imageView = view,
      .imageLayout = image_layout,
    };
    return descriptor;
  }

  void
  destroy()
  {
    if (view)
      vkDestroyImageView(device->device, view, nullptr);
    if (!created_from_image)
      vkDestroyImage(device->device, image, nullptr);
    if (sampler)
      vkDestroySampler(device->device, sampler, nullptr);
    if (device_memory)
      vkFreeMemory(device->device, device_memory, nullptr);
  }

  void
  load_from_ktx(const ktx_uint8_t *bytes,
                ktx_size_t size,
                vulkan_device *device,
                VkQueue copy_queue,
                VkFormat format)
  {
    ktxTexture *kTexture;
    KTX_error_code ktxresult;
    ktxresult = ktxTexture_CreateFromMemory(
      bytes, size, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

    if (KTX_SUCCESS != ktxresult) {
      xrg_log_e("Creation of ktxTexture failed: %d", ktxresult);
      return;
    }

    this->device = device;
    width = kTexture->baseWidth;
    height = kTexture->baseHeight;
    mip_levels = kTexture->numLevels;

    create_image(format);

    upload(kTexture, copy_queue);

    create_sampler();
    create_image_view(format);

    ktxTexture_Destroy(kTexture);
  }

  void
  load_from_ktx(VkImage image,
                const ktx_uint8_t *bytes,
                ktx_size_t size,
                vulkan_device *device,
                VkQueue copy_queue)
  {
    ktxTexture *kTexture;
    KTX_error_code ktxresult;
    ktxresult = ktxTexture_CreateFromMemory(
      bytes, size, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

    if (KTX_SUCCESS != ktxresult) {
      xrg_log_e("Creation of ktxTexture failed: %d", ktxresult);
      return;
    }

    this->device = device;
    width = kTexture->baseWidth;
    height = kTexture->baseHeight;
    mip_levels = kTexture->numLevels;

    this->image = image;

    upload_no_mem(kTexture, copy_queue);

    ktxTexture_Destroy(kTexture);

    created_from_image = true;
  }

  void
  create_image(VkFormat format)
  {
    VkImageCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = { .width = width, .height = height, .depth = 1 },
      .mipLevels = mip_levels,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
      info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vk_check(vkCreateImage(device->device, &info, nullptr, &image));
  }

  void
  upload(ktxTexture *tex, VkQueue copy_queue)
  {
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = ktxTexture_GetSize(tex),
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer staging_buffer;
    vk_check(vkCreateBuffer(device->device, &bufferCreateInfo, nullptr,
                            &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device->device, staging_buffer, &mem_reqs);

    uint32_t type_index = device->get_memory_type(
      mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo mem_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex = type_index,
    };

    VkDeviceMemory staging_memory;
    vk_check(
      vkAllocateMemory(device->device, &mem_info, nullptr, &staging_memory));
    vk_check(
      vkBindBufferMemory(device->device, staging_buffer, staging_memory, 0));

    uint8_t *data;
    vk_check(vkMapMemory(device->device, staging_memory, 0, mem_reqs.size, 0,
                         (void **)&data));

    KTX_error_code kResult;
    kResult = ktxTexture_LoadImageData(tex, data, (ktx_size_t)mem_reqs.size);
    if (kResult != KTX_SUCCESS)
      xrg_log_f("Could not load image data: %d", kResult);

    vkUnmapMemory(device->device, staging_memory);

    std::vector<VkBufferImageCopy> buffer_image_copies;
    for (uint32_t i = 0; i < mip_levels; i++) {
      ktx_size_t offset;
      ktxTexture_GetImageOffset(tex, i, 0, 0, &offset);
      VkBufferImageCopy image_copy = {
        .bufferOffset = offset,
        .imageSubresource =
        {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = i,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .imageExtent = {
          .width = tex->baseWidth >> i,
          .height = tex->baseHeight >> i,
          .depth = tex->baseDepth  >> i,
        },
      };
      buffer_image_copies.push_back(image_copy);
    }

    VkCommandBuffer copy_cmd =
      device->create_cmd_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    vkGetImageMemoryRequirements(device->device, image, &mem_reqs);

    mem_info.allocationSize = mem_reqs.size;

    mem_info.memoryTypeIndex = device->get_memory_type(
      mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk_check(
      vkAllocateMemory(device->device, &mem_info, nullptr, &device_memory));
    vk_check(vkBindImageMemory(device->device, image, device_memory, 0));

    VkImageSubresourceRange subresource_range = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = mip_levels,
      .layerCount = 1,
    };

    set_image_layout(copy_cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range, 0,
                     VK_ACCESS_TRANSFER_WRITE_BIT);

    vkCmdCopyBufferToImage(copy_cmd, staging_buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(buffer_image_copies.size()),
                           buffer_image_copies.data());

    image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    set_image_layout(copy_cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     subresource_range, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT);

    device->flush_cmd_buffer(copy_cmd, copy_queue);

    vkFreeMemory(device->device, staging_memory, nullptr);
    vkDestroyBuffer(device->device, staging_buffer, nullptr);
  }

  void
  upload_no_mem(ktxTexture *tex, VkQueue copy_queue)
  {
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = ktxTexture_GetSize(tex),
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer staging_buffer;
    vk_check(vkCreateBuffer(device->device, &bufferCreateInfo, nullptr,
                            &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device->device, staging_buffer, &mem_reqs);

    uint32_t type_index = device->get_memory_type(
      mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo mem_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex = type_index,
    };

    VkDeviceMemory staging_memory;
    vk_check(
      vkAllocateMemory(device->device, &mem_info, nullptr, &staging_memory));
    vk_check(
      vkBindBufferMemory(device->device, staging_buffer, staging_memory, 0));

    uint8_t *data;
    vk_check(vkMapMemory(device->device, staging_memory, 0, mem_reqs.size, 0,
                         (void **)&data));

    KTX_error_code kResult;
    kResult = ktxTexture_LoadImageData(tex, data, (ktx_size_t)mem_reqs.size);
    if (kResult != KTX_SUCCESS)
      xrg_log_f("Could not load image data: %d", kResult);

    vkUnmapMemory(device->device, staging_memory);

    std::vector<VkBufferImageCopy> buffer_image_copies;
    for (uint32_t i = 0; i < mip_levels; i++) {
      ktx_size_t offset;
      ktxTexture_GetImageOffset(tex, i, 0, 0, &offset);
      VkBufferImageCopy image_copy = {
        .bufferOffset = offset,
        .imageSubresource =
        {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = i,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
        .imageExtent = {
          .width = tex->baseWidth >> i,
          .height = tex->baseHeight >> i,
          .depth = tex->baseDepth  >> i,
        },
      };
      buffer_image_copies.push_back(image_copy);
    }

    VkCommandBuffer copy_cmd =
      device->create_cmd_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkImageSubresourceRange subresource_range = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = mip_levels,
      .layerCount = 1,
    };

    set_image_layout(copy_cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range, 0,
                     VK_ACCESS_TRANSFER_WRITE_BIT);

    vkCmdCopyBufferToImage(copy_cmd, staging_buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(buffer_image_copies.size()),
                           buffer_image_copies.data());

    image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    set_image_layout(copy_cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     subresource_range, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT);

    device->flush_cmd_buffer(copy_cmd, copy_queue);

    vkFreeMemory(device->device, staging_memory, nullptr);
    vkDestroyBuffer(device->device, staging_buffer, nullptr);
  }


  void
  create_sampler()
  {
    VkSamplerCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 8,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = (float)mip_levels,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };

    vk_check(vkCreateSampler(device->device, &info, nullptr, &sampler));
  }

  void
  create_image_view(VkFormat format)
  {
    VkImageViewCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vk_check(vkCreateImageView(device->device, &info, nullptr, &view));
  }
};
