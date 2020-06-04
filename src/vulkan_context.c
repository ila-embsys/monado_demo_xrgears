/*
 * xrgears
 *
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_context.h"

void
vulkan_context_destroy(vulkan_context *self)
{
  vkDestroyInstance(self->instance, NULL);
}

VkResult
vulkan_context_create_instance(vulkan_context *self)
{
  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "xrgears",
    .pEngineName = "xrgears",
    .apiVersion = VK_MAKE_VERSION(1, 0, 2),
  };

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };

  return vkCreateInstance(&instance_info, NULL, &self->instance);
}
