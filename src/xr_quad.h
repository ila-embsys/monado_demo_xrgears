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

typedef struct xr_quad
{
  XrCompositionLayerQuad layer;
  XrSwapchain swapchain;
  uint32_t swapchain_length;
  XrSwapchainImageVulkanKHR* images;
} xr_quad;

void
xr_quad_init(xr_quad* self,
             XrSession session,
             XrSpace space,
             XrExtent2Di extent,
             XrPosef pose,
             XrExtent2Df size);

bool
xr_quad_acquire_swapchain(xr_quad* self, uint32_t* buffer_index);

bool
xr_quad_release_swapchain(xr_quad* self);

#ifdef __cplusplus
}
#endif
