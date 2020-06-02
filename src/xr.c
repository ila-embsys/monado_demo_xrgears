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

#include "xr.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>

#include <openxr/openxr_reflection.h>

#include "log.h"

static const char* viewport_config_name = "/viewport_configuration/vr";

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char*
xr_result_to_string(XrResult result)
{
  switch (result) {

#define MAKE_CASE(VAL, _)                                                      \
  case VAL: return #VAL;

    XR_LIST_ENUM_XrResult(MAKE_CASE);
  default: return "UNKNOWN";
  }
}

bool
xr_result(XrResult result, const char* format, ...)
{
  if (XR_SUCCEEDED(result))
    return true;

  const char* resultString = xr_result_to_string(result);

  size_t len1 = strlen(format);
  size_t len2 = strlen(resultString) + 1;
  char formatRes[len1 + len2 + 4]; // + " []\n"
  sprintf(formatRes, "%s [%s]\n", format, resultString);

  va_list args;
  va_start(args, format);
  vprintf(formatRes, args);
  va_end(args);
  return false;
}

bool
is_extension_supported(char* name, XrExtensionProperties* props, uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
    if (!strcmp(name, props[i].extensionName))
      return true;
  return false;
}

static bool
_check_vk_extension()
{
  XrResult result;
  uint32_t instanceExtensionCount = 0;
  result = xrEnumerateInstanceExtensionProperties(
    NULL, 0, &instanceExtensionCount, NULL);

  if (!xr_result(result,
                 "Failed to enumerate number of instance extension properties"))
    return false;

  XrExtensionProperties instanceExtensionProperties[instanceExtensionCount];
  for (uint16_t i = 0; i < instanceExtensionCount; i++)
    instanceExtensionProperties[i] = (XrExtensionProperties){
      .type = XR_TYPE_EXTENSION_PROPERTIES,
    };

  result = xrEnumerateInstanceExtensionProperties(NULL, instanceExtensionCount,
                                                  &instanceExtensionCount,
                                                  instanceExtensionProperties);
  if (!xr_result(result, "Failed to enumerate extension properties"))
    return false;

  result =
    is_extension_supported(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
                           instanceExtensionProperties, instanceExtensionCount);
  if (!xr_result(result,
                 "Runtime does not support required instance extension %s\n",
                 XR_KHR_VULKAN_ENABLE_EXTENSION_NAME))
    return false;

  return true;
}

static bool
_enumerate_api_layers()
{
  uint32_t apiLayerCount;
  xrEnumerateApiLayerProperties(0, &apiLayerCount, NULL);

  XrApiLayerProperties apiLayerProperties[apiLayerCount];
  memset(apiLayerProperties, 0, apiLayerCount * sizeof(XrApiLayerProperties));

  for (uint32_t i = 0; i < apiLayerCount; i++) {
    apiLayerProperties[i].type = XR_TYPE_API_LAYER_PROPERTIES;
  }
  xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount,
                                apiLayerProperties);

  for (uint32_t i = 0; i < apiLayerCount; i++) {
    if (strcmp(apiLayerProperties->layerName, "XR_APILAYER_LUNARG_api_dump") ==
        0) {
      xrg_log_i("XR_APILAYER_LUNARG_api_dump supported.");
    } else if (strcmp(apiLayerProperties->layerName,
                      "XR_APILAYER_LUNARG_core_validation") == 0) {
      xrg_log_i("XR_APILAYER_LUNARG_core_validation supported.\n");
    }
  }

  return true;
}

static bool
_create_instance(xr_example* self)
{
  const char* const enabledExtensions[] = {
    XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
  };

  XrInstanceCreateInfo instanceCreateInfo = {
    .type = XR_TYPE_INSTANCE_CREATE_INFO,
    .createFlags = 0,
    .enabledExtensionCount = 1,
    .enabledExtensionNames = enabledExtensions,
    .enabledApiLayerCount = 0,
    .applicationInfo = {
      .applicationName = "xrgears",
      .engineName = "xrgears",
      .applicationVersion = 1,
      .engineVersion = 1,
      .apiVersion = XR_CURRENT_API_VERSION,
    },
  };

  XrResult result;
  result = xrCreateInstance(&instanceCreateInfo, &self->instance);
  if (!xr_result(result, "Failed to create XR instance."))
    return false;

  return true;
}

static bool
_create_system(xr_example* self)
{
  XrPath vrConfigName;
  XrResult result;
  result = xrStringToPath(self->instance, viewport_config_name, &vrConfigName);
  xr_result(result, "failed to get viewport configuration name");

  xrg_log_i("Got vrconfig %lu\n", vrConfigName);

  XrSystemGetInfo systemGetInfo = {
    .type = XR_TYPE_SYSTEM_GET_INFO,
    .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  result = xrGetSystem(self->instance, &systemGetInfo, &self->system_id);
  if (!xr_result(result, "Failed to get system for %s viewport configuration.",
                 viewport_config_name))
    return false;

  XrSystemProperties systemProperties = {
    .type = XR_TYPE_SYSTEM_PROPERTIES,
    .graphicsProperties = { 0 },
    .trackingProperties = { 0 },
  };

  result =
    xrGetSystemProperties(self->instance, self->system_id, &systemProperties);
  if (!xr_result(result, "Failed to get System properties"))
    return false;

  return true;
}

static bool
_set_up_views(xr_example* self)
{
  uint32_t viewConfigurationCount;
  XrResult result;
  result = xrEnumerateViewConfigurations(self->instance, self->system_id, 0,
                                         &viewConfigurationCount, NULL);
  if (!xr_result(result, "Failed to get view configuration count"))
    return false;

  XrViewConfigurationType viewConfigurations[viewConfigurationCount];
  result = xrEnumerateViewConfigurations(
    self->instance, self->system_id, viewConfigurationCount,
    &viewConfigurationCount, viewConfigurations);
  if (!xr_result(result, "Failed to enumerate view configurations!"))
    return false;

  self->view_config_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrViewConfigurationType optionalSecondaryViewConfigType =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;

  /* if struct (more specifically .type) is still 0 after searching, then
   we have not found the config. This way we don't need to set a bool
   found to true. */
  XrViewConfigurationProperties requiredViewConfigProperties = { 0 };
  XrViewConfigurationProperties secondaryViewConfigProperties = { 0 };

  for (uint32_t i = 0; i < viewConfigurationCount; ++i) {
    XrViewConfigurationProperties properties = {
      .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
    };

    result = xrGetViewConfigurationProperties(
      self->instance, self->system_id, viewConfigurations[i], &properties);
    if (!xr_result(result, "Failed to get view configuration info %d!", i))
      return false;

    if (viewConfigurations[i] == self->view_config_type &&
        properties.viewConfigurationType == self->view_config_type) {
      requiredViewConfigProperties = properties;
    } else if (viewConfigurations[i] == optionalSecondaryViewConfigType &&
               properties.viewConfigurationType ==
                 optionalSecondaryViewConfigType) {
      secondaryViewConfigProperties = properties;
    }
  }
  if (requiredViewConfigProperties.type !=
      XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
    xrg_log_e("Couldn't get required VR View Configuration %s from Runtime!",
              viewport_config_name);
    return false;
  }

  result = xrEnumerateViewConfigurationViews(self->instance, self->system_id,
                                             self->view_config_type, 0,
                                             &self->view_count, NULL);
  if (!xr_result(result, "Failed to get view configuration view count!"))
    return false;

  self->configuration_views =
    malloc(sizeof(XrViewConfigurationView) * self->view_count);

  result = xrEnumerateViewConfigurationViews(
    self->instance, self->system_id, self->view_config_type, self->view_count,
    &self->view_count, self->configuration_views);
  if (!xr_result(result, "Failed to enumerate view configuration views!"))
    return false;

  uint32_t secondaryViewConfigurationViewCount = 0;
  if (secondaryViewConfigProperties.type ==
      XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {

    result = xrEnumerateViewConfigurationViews(
      self->instance, self->system_id, optionalSecondaryViewConfigType, 0,
      &secondaryViewConfigurationViewCount, NULL);
    if (!xr_result(result, "Failed to get view configuration view count!"))
      return false;
  }

  if (secondaryViewConfigProperties.type ==
      XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
    result = xrEnumerateViewConfigurationViews(
      self->instance, self->system_id, optionalSecondaryViewConfigType,
      secondaryViewConfigurationViewCount, &secondaryViewConfigurationViewCount,
      self->configuration_views);
    if (!xr_result(result, "Failed to enumerate view configuration views!"))
      return false;
  }

  return true;
}

static bool
_check_graphics_api_support(xr_example* self)
{
  XrGraphicsRequirementsVulkanKHR vk_reqs = {
    .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
  };
  PFN_xrGetVulkanGraphicsRequirementsKHR GetVulkanGraphicsRequirements = NULL;
  XrResult result = xrGetInstanceProcAddr(
    self->instance, "xrGetVulkanGraphicsRequirementsKHR",
    (PFN_xrVoidFunction*)(&GetVulkanGraphicsRequirements));
  if (!xr_result(result, "Failed to retrieve OpenXR Vulkan function pointer!"))
    return false;

  result =
    GetVulkanGraphicsRequirements(self->instance, self->system_id, &vk_reqs);
  if (!xr_result(result, "Failed to get Vulkan graphics requirements!"))
    return false;

  XrVersion desired_version = XR_MAKE_VERSION(1, 0, 0);
  if (desired_version > vk_reqs.maxApiVersionSupported ||
      desired_version < vk_reqs.minApiVersionSupported) {
    xrg_log_e("Runtime does not support requested Vulkan version.");
    xrg_log_e("desired_version %lu", desired_version);
    xrg_log_e("minApiVersionSupported %lu", vk_reqs.minApiVersionSupported);
    xrg_log_e("maxApiVersionSupported %lu", vk_reqs.maxApiVersionSupported);
    return false;
  }
  return true;
}

static bool
_create_session(xr_example* self)
{
  XrSessionCreateInfo session_create_info = {
    .type = XR_TYPE_SESSION_CREATE_INFO,
    .next = &self->graphics_binding,
    .systemId = self->system_id,
  };

  XrResult result =
    xrCreateSession(self->instance, &session_create_info, &self->session);
  if (!xr_result(result, "Failed to create session"))
    return false;
  return true;
}

static bool
_check_supported_spaces(xr_example* self)
{
  uint32_t referenceSpacesCount;
  XrResult result =
    xrEnumerateReferenceSpaces(self->session, 0, &referenceSpacesCount, NULL);
  if (!xr_result(result, "Getting number of reference spaces failed!"))
    return false;

  XrReferenceSpaceType referenceSpaces[referenceSpacesCount];
  result = xrEnumerateReferenceSpaces(self->session, referenceSpacesCount,
                                      &referenceSpacesCount, referenceSpaces);
  if (!xr_result(result, "Enumerating reference spaces failed!"))
    return false;

  bool localSpaceSupported = false;
  xrg_log_i("Enumerated %d reference spaces.", referenceSpacesCount);
  for (uint32_t i = 0; i < referenceSpacesCount; i++) {
    if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL) {
      localSpaceSupported = true;
    }
  }

  if (!localSpaceSupported) {
    xrg_log_e("XR_REFERENCE_SPACE_TYPE_LOCAL unsupported.");
    return false;
  }

  XrPosef identity = {
    .orientation = { .x = 0, .y = 0, .z = 0, .w = 1.0 },
    .position = { .x = 0, .y = 0, .z = 0 },
  };

  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = identity,
  };

  result = xrCreateReferenceSpace(self->session, &referenceSpaceCreateInfo,
                                  &self->local_space);
  if (!xr_result(result, "Failed to create local space!"))
    return false;

  return true;
}

static bool
_begin_session(xr_example* self)
{
  XrSessionBeginInfo sessionBeginInfo = {
    .type = XR_TYPE_SESSION_BEGIN_INFO,
    .primaryViewConfigurationType = self->view_config_type,
  };
  XrResult result = xrBeginSession(self->session, &sessionBeginInfo);
  if (!xr_result(result, "Failed to begin session!"))
    return false;

  return true;
}

static bool
_create_swapchains(xr_example* self, xr_proj* proj)
{
  XrResult result;
  uint32_t swapchainFormatCount;
  result =
    xrEnumerateSwapchainFormats(self->session, 0, &swapchainFormatCount, NULL);
  if (!xr_result(result, "Failed to get number of supported swapchain formats"))
    return false;

  int64_t swapchainFormats[swapchainFormatCount];
  result = xrEnumerateSwapchainFormats(self->session, swapchainFormatCount,
                                       &swapchainFormatCount, swapchainFormats);
  if (!xr_result(result, "Failed to enumerate swapchain formats"))
    return false;

  /* First create swapchains and query the length for each swapchain. */
  proj->swapchains = malloc(sizeof(XrSwapchain) * self->view_count);

  proj->swapchain_length = malloc(sizeof(uint32_t) * self->view_count);

  self->swapchain_format = swapchainFormats[0];

  for (uint32_t i = 0; i < self->view_count; i++) {
    XrSwapchainCreateInfo swapchainCreateInfo = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
      .createFlags = 0,
      // just use the first enumerated format
      .format = swapchainFormats[0],
      .sampleCount = 1,
      .width = self->configuration_views[i].recommendedImageRectWidth,
      .height = self->configuration_views[i].recommendedImageRectHeight,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = 1,
    };

    xrg_log_i("Swapchain %d dimenstions: %dx%d", i,
              self->configuration_views[i].recommendedImageRectWidth,
              self->configuration_views[i].recommendedImageRectHeight);

    result = xrCreateSwapchain(self->session, &swapchainCreateInfo,
                               &proj->swapchains[i]);
    if (!xr_result(result, "Failed to create swapchain %d!", i))
      return false;

    result = xrEnumerateSwapchainImages(proj->swapchains[i], 0,
                                        &proj->swapchain_length[i], NULL);
    if (!xr_result(result, "Failed to enumerate swapchains"))
      return false;
  }

  // most likely all swapchains have the same length, but let's not fail
  // if they are not
  uint32_t maxSwapchainLength = 0;
  for (uint32_t i = 0; i < self->view_count; i++) {
    if (proj->swapchain_length[i] > maxSwapchainLength) {
      maxSwapchainLength = proj->swapchain_length[i];
    }
  }

  proj->images = malloc(sizeof(XrSwapchainImageVulkanKHR*) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; i++) {
    proj->images[i] =
      malloc(sizeof(XrSwapchainImageVulkanKHR) * maxSwapchainLength);
  }

  for (uint32_t i = 0; i < self->view_count; i++) {
    result = xrEnumerateSwapchainImages(
      proj->swapchains[i], proj->swapchain_length[i],
      &proj->swapchain_length[i], (XrSwapchainImageBaseHeader*)proj->images[i]);
    if (!xr_result(result, "Failed to enumerate swapchains"))
      return false;
    xrg_log_d("xrEnumerateSwapchainImages: swapchain_length[%d] %d", i,
              proj->swapchain_length[i]);
  }


  return true;
}

static void
_create_projection_views(xr_example* self, xr_proj* proj)
{
  proj->views =
    malloc(sizeof(XrCompositionLayerProjectionView) * self->view_count);

  for (uint32_t i = 0; i < self->view_count; i++)
    proj->views[i] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = {
        .swapchain = proj->swapchains[i],
        .imageRect = {
          .extent = {
              .width = (int32_t) self->configuration_views[i].recommendedImageRectWidth,
              .height = (int32_t) self->configuration_views[i].recommendedImageRectHeight,
          },
        },
      },
    };
}

bool
xr_begin_frame(xr_example* self)
{
  XrResult result;

  XrEventDataBuffer runtimeEvent = {
    .type = XR_TYPE_EVENT_DATA_BUFFER,
    .next = NULL,
  };

  self->frameState = (XrFrameState){
    .type = XR_TYPE_FRAME_STATE,
  };
  XrFrameWaitInfo frameWaitInfo = {
    .type = XR_TYPE_FRAME_WAIT_INFO,
  };
  result = xrWaitFrame(self->session, &frameWaitInfo, &self->frameState);
  if (!xr_result(result, "xrWaitFrame() was not successful, exiting..."))
    return false;

  XrResult pollResult = xrPollEvent(self->instance, &runtimeEvent);
  if (pollResult == XR_SUCCESS) {
    switch (runtimeEvent.type) {
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      XrEventDataSessionStateChanged* event =
        (XrEventDataSessionStateChanged*)&runtimeEvent;
      XrSessionState state = event->state;
      self->is_visible = event->state <= XR_SESSION_STATE_FOCUSED;
      xrg_log_d("EVENT: session state changed to %d. Visible: %d", state,
                self->is_visible);
      if (event->state >= XR_SESSION_STATE_STOPPING) { // TODO
        self->is_runnting = false;
      }
      break;
    }
    default: break;
    }
  } else if (pollResult == XR_EVENT_UNAVAILABLE) {
    // this is the usual case
  } else {
    xrg_log_e("Failed to poll events!\n");
    return false;
  }
  if (!self->is_visible)
    return false;

  // --- Create projection matrices and view matrices for each eye
  XrViewLocateInfo viewLocateInfo = {
    .type = XR_TYPE_VIEW_LOCATE_INFO,
    .displayTime = self->frameState.predictedDisplayTime,
    .space = self->local_space,
  };

  self->views = malloc(sizeof(XrView) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; i++) {
    self->views[i].type = XR_TYPE_VIEW;
  };

  XrViewState viewState = {
    .type = XR_TYPE_VIEW_STATE,
  };
  uint32_t viewCountOutput;
  result = xrLocateViews(self->session, &viewLocateInfo, &viewState,
                         self->view_count, &viewCountOutput, self->views);
  if (!xr_result(result, "Could not locate views"))
    return false;

  // --- Begin frame
  XrFrameBeginInfo frameBeginInfo = {
    .type = XR_TYPE_FRAME_BEGIN_INFO,
  };

  result = xrBeginFrame(self->session, &frameBeginInfo);
  if (!xr_result(result, "failed to begin frame!"))
    return false;

  return true;
}

bool
xr_aquire_swapchain(xr_example* self,
                    xr_proj* proj,
                    uint32_t i,
                    uint32_t* buffer_index)
{
  XrResult result;

  XrSwapchainImageAcquireInfo acquire_info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };

  result =
    xrAcquireSwapchainImage(proj->swapchains[i], &acquire_info, buffer_index);
  if (!xr_result(result, "failed to acquire swapchain image!"))
    return false;

  XrSwapchainImageWaitInfo wait_info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = INT64_MAX,
  };
  result = xrWaitSwapchainImage(proj->swapchains[i], &wait_info);
  if (!xr_result(result, "failed to wait for swapchain image!"))
    return false;

  proj->views[i].pose = self->views[i].pose;
  proj->views[i].fov = self->views[i].fov;
  proj->views[i].subImage.imageArrayIndex = *buffer_index;

  return true;
}

bool
xr_release_swapchain(XrSwapchain swapchain)
{
  XrSwapchainImageReleaseInfo info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  XrResult res = xrReleaseSwapchainImage(swapchain, &info);
  if (!xr_result(res, "failed to release swapchain image!"))
    return false;

  return true;
}

bool
xr_end_frame(xr_example* self)
{
  XrResult result;

  const XrCompositionLayerBaseHeader* const layers[] = {
    (const XrCompositionLayerBaseHeader* const) & self->sky.layer,
    (const XrCompositionLayerBaseHeader* const) & self->gears.layer,
    (const XrCompositionLayerBaseHeader* const) & self->quad.layer,
    (const XrCompositionLayerBaseHeader* const) & self->quad2.layer,
  };
  XrFrameEndInfo frameEndInfo = {
    .type = XR_TYPE_FRAME_END_INFO,
    .displayTime = self->frameState.predictedDisplayTime,
    .layerCount = ARRAY_SIZE(layers),
    .layers = layers,
    .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
  };

  result = xrEndFrame(self->session, &frameEndInfo);
  if (!xr_result(result, "failed to end frame!"))
    return false;

  free(self->views);

  return true;
}

static void
_cleanup_proj(xr_example* self, xr_proj* proj)
{
  for (uint32_t i = 0; i < self->view_count; i++) {
    xrDestroySwapchain(proj->swapchains[i]);
  }
  free(proj->swapchains);
}

void
xr_cleanup(xr_example* self)
{
  _cleanup_proj(self, &self->gears);
  _cleanup_proj(self, &self->sky);

  xrDestroySpace(self->local_space);
  xrDestroySession(self->session);
  xrDestroyInstance(self->instance);
}

static bool
_init_proj(xr_example* self, xr_proj* proj)
{
  if (!_create_swapchains(self, proj))
    return false;
  _create_projection_views(self, proj);
  proj->layer = (XrCompositionLayerProjection){
    .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
    .space = self->local_space,
    .viewCount = self->view_count,
    .views = proj->views,
  };
  return true;
}

bool
xr_init(xr_example* self,
        VkInstance instance,
        VkPhysicalDevice physical_device,
        VkDevice device,
        uint32_t queue_family_index,
        uint32_t queue_index)
{
  self->is_visible = true;
  self->is_runnting = true;

  if (!_check_vk_extension())
    return false;

  if (!_enumerate_api_layers())
    return false;

  if (!_create_instance(self))
    return false;

  if (!_create_system(self))
    return false;

  if (!_set_up_views(self))
    return false;

  if (!_check_graphics_api_support(self))
    return false;

  self->graphics_binding = (XrGraphicsBindingVulkanKHR){
    .type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
    .instance = instance,
    .physicalDevice = physical_device,
    .device = device,
    .queueFamilyIndex = queue_family_index,
    .queueIndex = queue_index,
  };

  if (!_create_session(self))
    return false;

  if (!_check_supported_spaces(self))
    return false;

  if (!_begin_session(self))
    return false;

  _init_proj(self, &self->gears);
  _init_proj(self, &self->sky);

  return true;
}
