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

#include <vector>
#include <assert.h>

#include "settings.hpp"

static VkBool32
validation_callback(VkDebugReportFlagsEXT flags,
                    VkDebugReportObjectTypeEXT obj_type,
                    uint64_t src_object,
                    size_t location,
                    int32_t msg_code,
                    const char *layer_prefix,
                    const char *msg,
                    void *user_data)
{
  (void)obj_type;
  (void)src_object;
  (void)location;
  (void)user_data;

  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    xrg_log_e("[%s] Code %d: %s", layer_prefix, msg_code, msg);
  else
    xrg_log_w("[%s] Code %d: %s", layer_prefix, msg_code, msg);

  return VK_FALSE;
}

class vulkan_instance
{
public:
  Settings *settings;
  std::vector<std::string> supported_extensions;

  VkInstance instance;
  VkDebugReportCallbackEXT validation_cb;

  vulkan_instance(Settings *settings)
  {
    this->settings = settings;
  }

  ~vulkan_instance()
  {
    if (settings->validation)
      destroy_validation_cb(instance);

    vkDestroyInstance(instance, nullptr);
  }

  VkResult
  create_instance()
  {
    query_supported_extensions();

    VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "xrgears",
      .pEngineName = "xrgears",
      .apiVersion = VK_MAKE_VERSION(1, 0, 2),
    };

    std::vector<const char *> extensions;
    if (settings->validation)
      enable_if_supported(&extensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

    VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = (uint32_t)extensions.size(),
      .ppEnabledExtensionNames = extensions.data(),
    };

    const char *validation_layers[] = {
      "VK_LAYER_KHRONOS_validation"
    };

    if (settings->validation) {
      instance_info.enabledLayerCount = 1;
      instance_info.ppEnabledLayerNames = validation_layers;
    }

    VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);

    if (settings->validation) {
      VkDebugReportFlagsEXT debug_report_flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
      create_validation_cb(instance, debug_report_flags);
    }

    return res;
  }

  void
  query_supported_extensions()
  {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (count > 0) {
      std::vector<VkExtensionProperties> extensions(count);
      if (vkEnumerateInstanceExtensionProperties(
            nullptr, &count, &extensions.front()) == VK_SUCCESS)
        for (auto ext : extensions)
          supported_extensions.push_back(ext.extensionName);
    }
  }

  bool
  enable_if_supported(std::vector<const char *> *extensions, const char *name)
  {
    if (is_extension_supported(name)) {
      xrg_log_d("instance: Enabling supported %s.", name);
      extensions->push_back(name);
      return true;
    } else {
      xrg_log_w("instance: %s not supported.", name);
      return false;
    }
  }

  bool
  is_extension_supported(std::string extension)
  {
    return std::find(supported_extensions.begin(), supported_extensions.end(),
                     extension) != supported_extensions.end();
  }

  void
  destroy_validation_cb(VkInstance instance)
  {
    if (validation_cb != VK_NULL_HANDLE) {
      PFN_vkDestroyDebugReportCallbackEXT destroy_cb =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          instance, "vkDestroyDebugReportCallbackEXT");

      destroy_cb(instance, validation_cb, nullptr);
    }
  }

  void
  create_validation_cb(VkInstance instance, VkDebugReportFlagsEXT flags)
  {
    PFN_vkCreateDebugReportCallbackEXT create_cb =
      (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackCreateInfoEXT debug_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
      .flags = flags,
      .pfnCallback = (PFN_vkDebugReportCallbackEXT)validation_callback,
    };

    VkResult err = create_cb(instance, &debug_info, nullptr, &validation_cb);
    assert(!err);
  }
};
