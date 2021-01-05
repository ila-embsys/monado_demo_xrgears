/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * @brief  OpenXR Vulkan example code.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "xr_equirect.h"

#include "xr.h"
#include "log.h"

static bool
_create_equirect_swapchain(xr_equirect* self,
                           XrSession session,
                           XrExtent2Di* extent)
{
  XrResult result;
  uint32_t swapchainFormatCount;
  result = xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, NULL);
  if (!xr_result(result, "Failed to get number of supported swapchain formats"))
    return false;

  int64_t swapchainFormats[swapchainFormatCount];
  result = xrEnumerateSwapchainFormats(session, swapchainFormatCount,
                                       &swapchainFormatCount, swapchainFormats);
  if (!xr_result(result, "Failed to enumerate swapchain formats"))
    return false;

  XrSwapchainCreateInfo swapchainCreateInfo = {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                  XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                  XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
    .createFlags = 0,
    // just use the first enumerated format
    .format = VK_FORMAT_R8G8B8A8_SRGB,
    .sampleCount = 1,
    .width = extent->width,
    .height = extent->height,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  result = xrCreateSwapchain(session, &swapchainCreateInfo, &self->swapchain);
  if (!xr_result(result, "Failed to create quad swapchain!"))
    return false;

  result = xrEnumerateSwapchainImages(self->swapchain, 0,
                                      &self->swapchain_length, NULL);
  if (!xr_result(result, "Failed to enumerate swapchains"))
    return false;

  xrg_log_d("quad_swapchain_length %d", self->swapchain_length);

  self->images =
    malloc(sizeof(XrSwapchainImageVulkanKHR) * self->swapchain_length);

  result = xrEnumerateSwapchainImages(
    self->swapchain, self->swapchain_length, &self->swapchain_length,
    (XrSwapchainImageBaseHeader*)self->images);
  if (!xr_result(result, "Failed to enumerate swapchain"))
    return false;

  xrg_log_d("new quad_swapchain_length %d", self->swapchain_length);

  return true;
}

bool
xr_equirect_acquire_swapchain(xr_equirect* self, uint32_t* buffer_index)
{
  XrResult result;

  XrSwapchainImageAcquireInfo swapchainImageAcquireInfo = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };

  result = xrAcquireSwapchainImage(self->swapchain, &swapchainImageAcquireInfo,
                                   buffer_index);
  if (!xr_result(result, "failed to acquire swapchain image!"))
    return false;

  XrSwapchainImageWaitInfo swapchainImageWaitInfo = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = INT64_MAX,
  };
  result = xrWaitSwapchainImage(self->swapchain, &swapchainImageWaitInfo);
  if (!xr_result(result, "failed to wait for swapchain image!"))
    return false;

  return true;
}

bool
xr_equirect_release_swapchain(xr_equirect* self)
{
  XrSwapchainImageReleaseInfo swapchainImageReleaseInfo = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  XrResult result =
    xrReleaseSwapchainImage(self->swapchain, &swapchainImageReleaseInfo);
  if (!xr_result(result, "failed to release swapchain image!"))
    return false;

  return true;
}

#define PI 3.142f

void
xr_equirect_init_v2(xr_equirect* self,
                    XrSession session,
                    XrSpace space,
                    XrExtent2Di extent,
                    XrPosef pose)
{
  bool res = _create_equirect_swapchain(self, session, &extent);
  xrg_log_i("Initialized equirect swapchain: %d", res);

  self->layer_v2 = (XrCompositionLayerEquirect2KHR){
    .type = XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR,
    .layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
    .space = space,
    .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
    .subImage = {
      .swapchain = self->swapchain,
      .imageRect = {
        .offset = { .x = 0, .y = 0 },
        .extent = extent,
      },
      .imageArrayIndex = 0,
    },
    .pose = pose,
    .radius = 1.0f,
    .centralHorizontalAngle = 2.0f * PI,
    .upperVerticalAngle = PI / 2.0,
    .lowerVerticalAngle = -PI / 2.0
  };

  // Don't fill the whole sphere.
  // self->layer.centralHorizontalAngle /= 2.0f;
  // self->layer.upperVerticalAngle /= 2.0f;
  // self->layer.lowerVerticalAngle /= 2.0f;
}


void
xr_equirect_init_v1(xr_equirect* self,
                    XrSession session,
                    XrSpace space,
                    XrExtent2Di extent,
                    XrPosef pose)
{
  bool res = _create_equirect_swapchain(self, session, &extent);
  xrg_log_i("Initialized equirect swapchain: %d", res);

  self->layer_v1 = (XrCompositionLayerEquirectKHR){
    .type = XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR,
    .layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
    .space = space,
    .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
    .subImage = {
      .swapchain = self->swapchain,
      .imageRect = {
        .offset = { .x = 0, .y = 0 },
        .extent = extent,
      },
      .imageArrayIndex = 0,
    },
    .pose = pose,
    .radius = 1.0f,

    .scale = { .x = 1, .y = 1 },
    .bias = { .x = 0, .y = 0 }
  };
}
