/*
 * xrgears
 *
 * Copyright 2019 Collabora Ltd.
 *
 * @brief  OpenXR Vulkan example code.
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#include <vulkan/vulkan.h>

#define XR_USE_PLATFORM_XLIB 1
#define XR_USE_GRAPHICS_API_VULKAN 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xr_example
{
  XrInstance instance;
  XrSession session;
  XrSpace local_space;
  XrSystemId system_id;
  XrViewConfigurationType view_config_type;

  XrSwapchain* swapchains;
  XrCompositionLayerProjectionView* projection_views;
  XrViewConfigurationView* configuration_views;

  XrGraphicsBindingVulkanKHR graphics_binding;

  XrSwapchainImageVulkanKHR** images;

  uint32_t view_count;

  bool is_visible;
  bool is_runnting;

  XrCompositionLayerProjection projectionLayer;
  XrFrameState frameState;
  XrView* views;

  int64_t swapchain_format;

} xr_example;

bool
xr_init(xr_example* self,
        VkInstance instance,
        VkPhysicalDevice physical_device,
        VkDevice device,
        uint32_t queue_family_index,
        uint32_t queue_index);

void
xr_cleanup(xr_example* self);

bool
xr_begin_frame(xr_example* self);

bool
xr_aquire_swapchain(xr_example* self, uint32_t i, uint32_t* buffer_index);

bool
xr_release_swapchain(xr_example* self, uint32_t eye);

bool
xr_end_frame(xr_example* self);

#ifdef __cplusplus
}
#endif
