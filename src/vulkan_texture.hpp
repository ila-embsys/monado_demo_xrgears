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
#include "vulkan_buffer.h"
#include "log.h"
#include "ktx_texture.h"



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
  get_descriptor();

  void
  destroy();

  void
  load_from_ktx(const ktx_uint8_t *bytes,
                ktx_size_t size,
                vulkan_device *device,
                VkQueue copy_queue,
                VkFormat format);

  void
  load_from_ktx(VkImage image,
                const ktx_uint8_t *bytes,
                ktx_size_t size,
                vulkan_device *device,
                VkQueue copy_queue);

  void
  create_image(VkFormat format);

  void
  upload(ktxTexture *tex, VkQueue copy_queue);

  void
  upload_no_mem(ktxTexture *tex, VkQueue copy_queue);


  void
  create_sampler();

  void
  create_image_view(VkFormat format);
};
