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

#include <vulkan/vulkan.h>

#include "vulkan_device.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  VkDevice device;

  VkImageView color_view;

  struct
  {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
  } depth;

  uint32_t width, height;
  VkFramebuffer frame_buffer;
  VkRenderPass render_pass;
} vulkan_framebuffer;

vulkan_framebuffer*
vulkan_framebuffer_create(VkDevice d);

void
vulkan_framebuffer_destroy(vulkan_framebuffer* self);


void
vulkan_framebuffer_init(vulkan_framebuffer* self,
                        vulkan_device* vulkanDevice,
                        VkImage color_image,
                        VkFormat color_format,
                        uint32_t width,
                        uint32_t height);

void
vulkan_framebuffer_begin_render_pass(vulkan_framebuffer* self,
                                     VkCommandBuffer cmdBuffer);

void
vulkan_framebuffer_set_viewport_and_scissor(vulkan_framebuffer* self,
                                            VkCommandBuffer cmdBuffer);

#ifdef __cplusplus
}
#endif
