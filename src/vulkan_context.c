/*
 * xrgears
 *
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_context.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void
vulkan_context_destroy(vulkan_context* self)
{
  vkDestroyInstance(self->instance, NULL);
}

VkResult
vulkan_context_create_instance(vulkan_context* self)
{
  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "xrgears",
    .pEngineName = "xrgears",
    .apiVersion = VK_MAKE_VERSION(1, 0, 2),
  };

  char* enabled_extensions[] = {
    VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
  };

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = ARRAY_SIZE(enabled_extensions),
    .ppEnabledExtensionNames = (const char* const*)enabled_extensions,
  };

  return vkCreateInstance(&instance_info, NULL, &self->instance);
}
