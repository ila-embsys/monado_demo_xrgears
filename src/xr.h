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

#include "settings.h"

#include <vulkan/vulkan.h>

#ifdef XR_OS_ANDROID
#include <android_native_app_glue.h>
#include <jni.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "vulkan_device.h"

#include "xr_quad.h"

#include "xr_equirect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xr_proj
{
  XrCompositionLayerProjection layer;
  XrCompositionLayerDepthInfoKHR depth_layer;
  XrCompositionLayerProjectionView* views;
  XrSwapchain* swapchains;
  uint32_t* swapchain_length; // One length per view
  uint32_t* last_acquired;

  bool has_depth;
  XrSwapchain* depth_swapchains;
  uint32_t* depth_swapchain_length;
  uint32_t* depth_last_acquired;

  XrSwapchainImageVulkanKHR** images;

  XrSwapchainImageVulkanKHR** depth_images;

} xr_proj;

typedef enum {
  SKY_TYPE_OFF,
  SKY_TYPE_PROJECTION,
  SKY_TYPE_EQUIRECT1,
  SKY_TYPE_EQUIRECT2
} xr_sky_layer_type;

typedef struct xr_example
{
  struct
  {
    bool equirect2;
    bool equirect1;
    bool overlay;
    bool depth_layer;
  } extensions;

  bool main_session_visible;

  XrInstance instance;
  XrSession session;
  XrSpace local_space;
  XrSystemId system_id;

  XrGraphicsBindingVulkanKHR graphics_binding;
  XrViewConfigurationType view_config_type;
  XrViewConfigurationView* configuration_views;


  // gears layer
  xr_proj gears;


  xr_proj sky;

  uint32_t view_count;

  bool is_visible;
  bool is_runnting;

  XrFrameState frameState;
  XrView* views;

  float near_z;
  float far_z;

  int64_t swapchain_format;
  int64_t depth_swapchain_format;


  // quad layer
  xr_quad quad;
  xr_quad quad2;


  xr_sky_layer_type sky_type;

  xr_equirect equirect;

  const XrCompositionLayerBaseHeader** layers;
  uint32_t num_layers;

  xrg_settings* settings;

#ifdef XR_OS_ANDROID
  XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid;
#endif

} xr_example;

bool
xr_init(xr_example* self,
        VkInstance instance,
        VkPhysicalDevice* physical_device);

bool
xr_init2(xr_example* self, VkInstance* instance, vulkan_device** vulkan_device);

bool
xr_init_post_vk(xr_example* self,
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
xr_proj_acquire_swapchain(xr_example* self, xr_proj* proj, uint32_t i);

bool
xr_proj_release_swapchain(xr_example* self, xr_proj* proj, uint32_t i);

bool
xr_end_frame(xr_example* self);

bool
xr_result(XrResult result, const char* format, ...);

#ifdef XR_OS_ANDROID
bool
xr_init_android(xr_example* self, struct android_app *app);
#endif

#ifdef __cplusplus
}
#endif
