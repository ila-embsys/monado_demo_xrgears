/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#include <vulkan/vulkan.h>

#ifdef XR_OS_ANDROID
#include <jni.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xr_equirect
{
  XrCompositionLayerEquirect2KHR layer_v2;
  XrCompositionLayerEquirectKHR layer_v1;
  XrSwapchain swapchain;
  uint32_t swapchain_length;
  XrSwapchainImageVulkanKHR* images;
} xr_equirect;

void
xr_equirect_init_v2(xr_equirect* self,
                    XrSession session,
                    XrSpace space,
                    XrExtent2Di extent,
                    XrPosef pose);

void
xr_equirect_init_v1(xr_equirect* self,
                    XrSession session,
                    XrSpace space,
                    XrExtent2Di extent,
                    XrPosef pose);

bool
xr_equirect_acquire_swapchain(xr_equirect* self, uint32_t* buffer_index);

bool
xr_equirect_release_swapchain(xr_equirect* self);

#ifdef __cplusplus
}
#endif
