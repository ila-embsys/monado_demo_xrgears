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

#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "log.h"
#include "ktx_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  vulkan_device *device;

  VkImage image;
  VkImageLayout image_layout;
  VkDeviceMemory device_memory;
  VkSampler sampler;
  VkImageView view;

  uint32_t width, height;
  uint32_t mip_levels;
  uint32_t layer_count;

  bool created_from_image;
} vulkan_texture;

VkDescriptorImageInfo
vulkan_texture_get_descriptor(vulkan_texture *self);

void
vulkan_texture_destroy(vulkan_texture *self);

void
vulkan_texture_load_ktx(vulkan_texture *self,
                        const ktx_uint8_t *bytes,
                        ktx_size_t size,
                        vulkan_device *device,
                        VkQueue copy_queue,
                        VkFormat format,
                        VkImageLayout dest_layout);

void
vulkan_texture_load_ktx_from_image(vulkan_texture *self,
                                   VkImage image,
                                   const ktx_uint8_t *bytes,
                                   ktx_size_t size,
                                   vulkan_device *device,
                                   VkQueue copy_queue,
                                   VkImageLayout dest_layout);

#ifdef __cplusplus
}
#endif
