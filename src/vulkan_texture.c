/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_texture.h"

#include "log.h"

static VkAccessFlags
_get_access_flags(VkImageLayout layout)
{
  switch (layout) {
  case VK_IMAGE_LAYOUT_UNDEFINED: return 0;
  case VK_IMAGE_LAYOUT_PREINITIALIZED: return VK_ACCESS_HOST_WRITE_BIT;
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_READ_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    return VK_ACCESS_TRANSFER_WRITE_BIT;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    return VK_ACCESS_SHADER_READ_BIT;
  default: xrg_log_e("Unhandled access mask case for layout %d.", layout);
  }
  return 0;
}

static void
_set_image_layout(VkCommandBuffer cmd_buffer,
                  VkImage image,
                  VkImageLayout src_layout,
                  VkImageLayout dst_layout,
                  VkImageSubresourceRange subresource_range)
{
  VkImageMemoryBarrier imageMemoryBarrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = _get_access_flags(src_layout),
    .dstAccessMask = _get_access_flags(dst_layout),
    .oldLayout = src_layout,
    .newLayout = dst_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = subresource_range,
  };

  vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL,
                       1, &imageMemoryBarrier);
}

VkDescriptorImageInfo
vulkan_texture_get_descriptor(vulkan_texture *self)
{
  VkDescriptorImageInfo descriptor = {
    .sampler = self->sampler,
    .imageView = self->view,
    .imageLayout = self->image_layout,
  };
  return descriptor;
}

void
vulkan_texture_destroy(vulkan_texture *self)
{
  if (self->view)
    vkDestroyImageView(self->device->device, self->view, NULL);
  if (!self->created_from_image)
    vkDestroyImage(self->device->device, self->image, NULL);
  if (self->sampler)
    vkDestroySampler(self->device->device, self->sampler, NULL);
  if (self->device_memory)
    vkFreeMemory(self->device->device, self->device_memory, NULL);
}

static void
_create_image(vulkan_texture *self, VkFormat format)
{
  VkImageCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { .width = self->width, .height = self->height, .depth = 1 },
    .mipLevels = self->mip_levels,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  vk_check(vkCreateImage(self->device->device, &info, NULL, &self->image));
}

static void
_load_ktx_to_staging_mem(vulkan_texture *self,
                         ktxTexture *tex,
                         VkMemoryRequirements *out_mem_reqs,
                         VkMemoryAllocateInfo *out_mem_info,
                         VkBuffer *out_staging_buffer,
                         VkDeviceMemory *out_staging_memory,
                         VkBufferImageCopy *buffer_image_copies)
{
  VkBufferCreateInfo bufferCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = ktxTexture_GetSize(tex),
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };

  vk_check(vkCreateBuffer(self->device->device, &bufferCreateInfo, NULL,
                          out_staging_buffer));

  vkGetBufferMemoryRequirements(self->device->device, *out_staging_buffer,
                                out_mem_reqs);

  uint32_t type_index;
  vulkan_device_get_memory_type(self->device, out_mem_reqs->memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &type_index);

  *out_mem_info = (VkMemoryAllocateInfo){
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = out_mem_reqs->size,
    .memoryTypeIndex = type_index,
  };

  vk_check(vkAllocateMemory(self->device->device, out_mem_info, NULL,
                            out_staging_memory));
  vk_check(vkBindBufferMemory(self->device->device, *out_staging_buffer,
                              *out_staging_memory, 0));

  uint8_t *data;
  vk_check(vkMapMemory(self->device->device, *out_staging_memory, 0,
                       out_mem_reqs->size, 0, (void **)&data));

  KTX_error_code kResult;
  kResult = ktxTexture_LoadImageData(tex, data, (ktx_size_t)out_mem_reqs->size);
  if (kResult != KTX_SUCCESS)
    xrg_log_f("Could not load image data: %d", kResult);

  vkUnmapMemory(self->device->device, *out_staging_memory);

  for (uint32_t i = 0; i < self->mip_levels; i++) {
    ktx_size_t offset;
    ktxTexture_GetImageOffset(tex, i, 0, 0, &offset);
    buffer_image_copies[i] = (VkBufferImageCopy) {
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
  }
}

static void
_allocate_image_memory(VkImage image,
                       vulkan_device *device,
                       VkMemoryRequirements *mem_reqs,
                       VkMemoryAllocateInfo *mem_info,
                       VkDeviceMemory *out_memory)
{
  vkGetImageMemoryRequirements(device->device, image, mem_reqs);

  mem_info->allocationSize = mem_reqs->size;

  vulkan_device_get_memory_type(device, mem_reqs->memoryTypeBits,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                &mem_info->memoryTypeIndex);
  vk_check(vkAllocateMemory(device->device, mem_info, NULL, out_memory));
  vk_check(vkBindImageMemory(device->device, image, *out_memory, 0));
}

static void
_transfer_image(vulkan_texture *self,
                VkQueue copy_queue,
                VkBuffer staging_buffer,
                VkImageLayout dest_layout,
                VkBufferImageCopy *buffer_image_copies)
{
  VkImageSubresourceRange subresource_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = self->mip_levels,
    .layerCount = 1,
  };

  VkCommandBuffer copy_cmd = vulkan_device_create_cmd_buffer(self->device);
  _set_image_layout(copy_cmd, self->image, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range);

  vkCmdCopyBufferToImage(copy_cmd, staging_buffer, self->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, self->mip_levels,
                         buffer_image_copies);

  self->image_layout = dest_layout;
  _set_image_layout(copy_cmd, self->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    dest_layout, subresource_range);

  vulkan_device_flush_cmd_buffer(self->device, copy_cmd, copy_queue);
}

static void
_upload(vulkan_texture *self,
        ktxTexture *tex,
        VkQueue copy_queue,
        bool alloc_mem,
        VkImageLayout dest_layout)
{
  VkMemoryRequirements mem_reqs;
  VkMemoryAllocateInfo mem_info;
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;

  VkBufferImageCopy *buffer_image_copies =
    malloc(sizeof(VkBufferImageCopy) * self->mip_levels);

  _load_ktx_to_staging_mem(self, tex, &mem_reqs, &mem_info, &staging_buffer,
                           &staging_memory, buffer_image_copies);

  if (alloc_mem)
    _allocate_image_memory(self->image, self->device, &mem_reqs, &mem_info,
                           &self->device_memory);

  _transfer_image(self, copy_queue, staging_buffer, dest_layout,
                  buffer_image_copies);

  vkFreeMemory(self->device->device, staging_memory, NULL);
  vkDestroyBuffer(self->device->device, staging_buffer, NULL);
}

static void
_create_sampler(vulkan_texture *self)
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
    .maxLod = (float)self->mip_levels,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
  };

  vk_check(vkCreateSampler(self->device->device, &info, NULL, &self->sampler));
}

static void
_create_image_view(vulkan_texture *self, VkFormat format)
{
  VkImageViewCreateInfo info = {
  .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
  .image = self->image,
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
    .levelCount = self->mip_levels,
    .baseArrayLayer = 0,
    .layerCount = 1,
  },
};

  vk_check(vkCreateImageView(self->device->device, &info, NULL, &self->view));
}

void
vulkan_texture_load_ktx(vulkan_texture *self,
                        const ktx_uint8_t *bytes,
                        ktx_size_t size,
                        vulkan_device *device,
                        VkQueue copy_queue,
                        VkFormat format,
                        VkImageLayout dest_layout)
{
  ktxTexture *kTexture;
  KTX_error_code ktxresult;
  ktxresult = ktxTexture_CreateFromMemory(
    bytes, size, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

  if (KTX_SUCCESS != ktxresult) {
    xrg_log_e("Creation of ktxTexture failed: %d", ktxresult);
    return;
  }

  self->device = device;
  self->width = kTexture->baseWidth;
  self->height = kTexture->baseHeight;
  self->mip_levels = kTexture->numLevels;

  _create_image(self, format);
  _upload(self, kTexture, copy_queue, true, dest_layout);
  _create_sampler(self);
  _create_image_view(self, format);

  ktxTexture_Destroy(kTexture);

  self->created_from_image = false;
}

void
vulkan_texture_load_ktx_from_image(vulkan_texture *self,
                                   VkImage image,
                                   const ktx_uint8_t *bytes,
                                   ktx_size_t size,
                                   vulkan_device *device,
                                   VkQueue copy_queue,
                                   VkImageLayout dest_layout)
{
  ktxTexture *kTexture;
  KTX_error_code ktxresult;
  ktxresult = ktxTexture_CreateFromMemory(
    bytes, size, KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

  if (KTX_SUCCESS != ktxresult) {
    xrg_log_e("Creation of ktxTexture failed: %d", ktxresult);
    return;
  }

  self->device = device;
  self->width = kTexture->baseWidth;
  self->height = kTexture->baseHeight;
  self->mip_levels = kTexture->numLevels;

  self->image = image;

  _upload(self, kTexture, copy_queue, false, dest_layout);

  ktxTexture_Destroy(kTexture);

  self->created_from_image = true;
}
