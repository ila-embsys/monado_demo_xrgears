/*
 * xrgears
 *
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include <vulkan/vulkan.h>

class vulkan_instance
{
public:
  VkInstance instance;

  vulkan_instance() {}

  ~vulkan_instance()
  {
    vkDestroyInstance(instance, nullptr);
  }

  VkResult
  create_instance()
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

    return vkCreateInstance(&instance_info, nullptr, &instance);
  }
};
