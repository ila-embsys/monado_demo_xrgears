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
  xrg_log_e(formatRes, args);
  va_end(args);
  return false;
}

bool
is_extension_supported(const char* name,
                       XrExtensionProperties* props,
                       uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
    if (!strcmp(name, props[i].extensionName))
      return true;
  return false;
}

static void
print_supported_extensions(XrExtensionProperties* props,
                           uint32_t count)
{
  xrg_log_d("== Supported OpenXR extensions ==");
  for (uint32_t i = 0; i < count; i++)
    xrg_log_d("%s", props[i].extensionName);
}

static bool
_check_xr_extensions(xr_example* self, const char* vulkan_extension)
{
  XrResult result;
  uint32_t count = 0;
  result = xrEnumerateInstanceExtensionProperties(NULL, 0, &count, NULL);

  if (!xr_result(result, "Failed to enumerate instance extensions."))
    return false;

  XrExtensionProperties props[count];
  for (uint16_t i = 0; i < count; i++)
    props[i] = (XrExtensionProperties){
      .type = XR_TYPE_EXTENSION_PROPERTIES,
    };

  result = xrEnumerateInstanceExtensionProperties(NULL, count, &count, props);
  if (!xr_result(result, "Failed to enumerate extension properties"))
    return false;

  print_supported_extensions(props, count);

  if (!is_extension_supported(vulkan_extension, props, count)) {
    xrg_log_e("Runtime does not support required instance extension %s",
              vulkan_extension);
    return false;
  }

  // try equirect2 ext for sky first, then equirect1, lastly projection.
  if (is_extension_supported(XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME,
                             props, count)) {
    self->extensions.equirect2 = true;
    self->sky_type = SKY_TYPE_EQUIRECT2;
    xrg_log_i("Will use equirect2 layer for sky rendering.");
  } else {
    xrg_log_w("%s extension unsupported.",
              XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME);
  }

  if (!self->extensions.equirect2) {
    if (is_extension_supported(XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME,
                               props, count)) {
      self->sky_type = SKY_TYPE_EQUIRECT1;
      xrg_log_i("Will use equirect1 layer for sky rendering.");
      self->extensions.equirect1 = true;
    } else {
      xrg_log_w("%s extension unsupported.",
                XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME);
    }
  }

  if (!self->extensions.equirect2 && !self->extensions.equirect1) {
    self->sky_type = SKY_TYPE_PROJECTION;
    xrg_log_i("Will use projection layer for sky rendering.");
  }

  if (self->settings->enable_overlay) {
    self->extensions.overlay =
      is_extension_supported(XR_EXTX_OVERLAY_EXTENSION_NAME, props, count);
    xrg_log_i("Runtime support for instance extension %s: %d",
              XR_EXTX_OVERLAY_EXTENSION_NAME, self->extensions.overlay);
  }

  if (!is_extension_supported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, props, count)) {
    self->extensions.depth_layer = true;;
    xrg_log_i("Runtime does not support depth layer extension %s",
              XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    // not fatal
  }

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
_create_instance(xr_example* self, char* vulkan_extension)
{
  const char* enabledExtensions[5] = { vulkan_extension };
  uint32_t num_extensions = 1;

  // only enables either equirect2 or equirect1, not both
  if (self->extensions.equirect2) {
    enabledExtensions[num_extensions++] = XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME;
  } else if (self->extensions.equirect1) {
    enabledExtensions[num_extensions++] = XR_KHR_COMPOSITION_LAYER_EQUIRECT_EXTENSION_NAME;
  }

  if (self->extensions.overlay) {
    enabledExtensions[num_extensions++] = XR_EXTX_OVERLAY_EXTENSION_NAME;
  }

  if (self->extensions.depth_layer) {
    enabledExtensions[num_extensions++] = XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME;
  }

#ifdef XR_OS_ANDROID
  enabledExtensions[num_extensions++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
#endif

  XrInstanceCreateInfo instanceCreateInfo = {
    .type = XR_TYPE_INSTANCE_CREATE_INFO,
#ifdef XR_OS_ANDROID
    .next = &self->instanceCreateInfoAndroid,
#endif
    .createFlags = 0,
    .applicationInfo =
      (XrApplicationInfo){
        .applicationName = "xrgears",
        .applicationVersion = 1,
        .engineName = "xrgears",
        .engineVersion = 1,
        .apiVersion = XR_CURRENT_API_VERSION,
      },
    .enabledApiLayerCount = 0,
    .enabledApiLayerNames = NULL,
    .enabledExtensionCount = num_extensions,
    .enabledExtensionNames = enabledExtensions,
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
  XrResult result;
  XrSystemGetInfo systemGetInfo = {
    .type = XR_TYPE_SYSTEM_GET_INFO,
    .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  result = xrGetSystem(self->instance, &systemGetInfo, &self->system_id);
  if (!xr_result(result, "Failed to get system"))
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
    xrg_log_e("Couldn't get required VR View Configuration from Runtime!");
    return false;
  }

  result = xrEnumerateViewConfigurationViews(self->instance, self->system_id,
                                             self->view_config_type, 0,
                                             &self->view_count, NULL);
  if (!xr_result(result, "Failed to get view configuration view count!"))
    return false;

  self->configuration_views = (XrViewConfigurationView*)malloc(
    sizeof(XrViewConfigurationView) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; ++i) {
    self->configuration_views[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    self->configuration_views[i].next = NULL;
  }

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
_check_graphics_api_support2(xr_example* self)
{
  // XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR is aliased to
  // XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR
  XrGraphicsRequirementsVulkan2KHR vk_reqs = {
    .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
  };
  PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2 = NULL;
  XrResult result =
    xrGetInstanceProcAddr(self->instance, "xrGetVulkanGraphicsRequirements2KHR",
                          (PFN_xrVoidFunction*)&GetVulkanGraphicsRequirements2);
  if (!xr_result(result, "Failed to load xrGetVulkanGraphicsRequirements2KHR."))
    return false;

  result =
    GetVulkanGraphicsRequirements2(self->instance, self->system_id, &vk_reqs);
  if (!xr_result(result, "Failed to get Vulkan graphics requirements!"))
    return false;

  xrg_log_i("XrGraphicsRequirementsVulkan2KHR:");
  xrg_log_i("minApiVersionSupported: %d", vk_reqs.minApiVersionSupported);
  xrg_log_i("maxApiVersionSupported: %d", vk_reqs.maxApiVersionSupported);

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
_create_vk_instance2(xr_example* self, VkInstance* instance)
{
  //! @todo merge with vulkan_device_create_device()
  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "xrgears",
    .pEngineName = "xrgears",
    .apiVersion = VK_MAKE_VERSION(1, 0, 2),
  };

  // runtime will add extensions it requires
  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = 0,
    .ppEnabledExtensionNames = NULL,
  };

  XrVulkanInstanceCreateInfoKHR xr_vk_instance_info = {
    .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
    .next = NULL,
    .createFlags = 0,
    .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
    .systemId = self->system_id,
    .vulkanCreateInfo = &instance_info,
    .vulkanAllocator = NULL
  };


  PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR = NULL;
  XrResult result =
    xrGetInstanceProcAddr(self->instance, "xrCreateVulkanInstanceKHR",
                          (PFN_xrVoidFunction*)&CreateVulkanInstanceKHR);
  if (!xr_result(result, "Failed to load xrCreateVulkanInstanceKHR."))
    return false;

  VkResult vk_result;
  result = CreateVulkanInstanceKHR(self->instance, &xr_vk_instance_info,
                                   instance, &vk_result);

  if (!xr_result(result, "Failed to create Vulkan instance!"))
    return false;

  if (vk_result != VK_SUCCESS) {
    xrg_log_e("Runtime failed to create Vulkan instance: %d\n", vk_result);
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
  XrResult result =
    xrGetInstanceProcAddr(self->instance, "xrGetVulkanGraphicsRequirementsKHR",
                          (PFN_xrVoidFunction*)&GetVulkanGraphicsRequirements);
  if (!xr_result(result, "Failed to load xrGetVulkanGraphicsRequirementsKHR."))
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
_get_vk_instance_extensions(xr_example* self)
{
  PFN_xrGetVulkanInstanceExtensionsKHR fun = NULL;
  XrResult res =
    xrGetInstanceProcAddr(self->instance, "xrGetVulkanInstanceExtensionsKHR",
                          (PFN_xrVoidFunction*)&fun);
  if (!xr_result(res, "Failed to load xrGetVulkanInstanceExtensionsKHR."))
    return false;

  uint32_t size = 0;
  res = fun(self->instance, self->system_id, 0, &size, NULL);

  char* names = (char*)malloc(sizeof(char) * size);
  fun(self->instance, self->system_id, size, &size, names);

  xrg_log_i("xrGetVulkanInstanceExtensionsKHR: %s", names);

  return true;
}

static bool
_init_vk_device(xr_example* self,
                VkInstance vk_instance,
                VkPhysicalDevice* physical_device)
{

  PFN_xrGetVulkanGraphicsDeviceKHR fun = NULL;
  XrResult res = xrGetInstanceProcAddr(
    self->instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&fun);

  if (!xr_result(res, "Failed to load xrGetVulkanGraphicsDeviceKHR."))
    return false;

  res = fun(self->instance, self->system_id, vk_instance, physical_device);

  if (!xr_result(res, "Failed to get Vulkan graphics device."))
    return false;

  return true;
}

static bool
_get_vk_device2(xr_example* self,
                VkInstance vk_instance,
                VkPhysicalDevice* physical_device)
{
  PFN_xrGetVulkanGraphicsDevice2KHR fun = NULL;
  XrResult res = xrGetInstanceProcAddr(
    self->instance, "xrGetVulkanGraphicsDevice2KHR", (PFN_xrVoidFunction*)&fun);

  if (!xr_result(res, "Failed to load xrGetVulkanGraphicsDevice2KHR."))
    return false;

  XrVulkanGraphicsDeviceGetInfoKHR info = {
    .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
    .next = NULL,
    .systemId = self->system_id,
    .vulkanInstance = vk_instance
  };

  res = fun(self->instance, &info, physical_device);

  if (!xr_result(res, "Failed to get Vulkan graphics device."))
    return false;

  return true;
}

static bool
_create_vk_device2(xr_example* self,
                   VkPhysicalDevice physical_device,
                   vulkan_device** device)
{

  *device = vulkan_device_create(physical_device);

  vulkan_device* d = *device;

  PFN_xrCreateVulkanDeviceKHR fun = NULL;
  XrResult res = xrGetInstanceProcAddr(
    self->instance, "xrCreateVulkanDeviceKHR", (PFN_xrVoidFunction*)&fun);

  if (!xr_result(res, "Failed to load xrCreateVulkanDeviceKHR."))
    return false;


  //! @todo merge with vulkan_device_create_device()
  VkDeviceQueueCreateInfo queue_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = d->graphics_family_index,
    .queueCount = 1,
    .pQueuePriorities = (float[]){ 0.0f },
  };

  VkPhysicalDeviceFeatures enabled_features = {
    .samplerAnisotropy = VK_TRUE,
  };

  // runtime will add extensions it requires
  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
    .pEnabledFeatures = &enabled_features,
    .enabledExtensionCount = 0,
    .ppEnabledExtensionNames = NULL,
  };


  XrVulkanDeviceCreateInfoKHR info = {
    .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
    .next = NULL,
    .systemId = self->system_id,
    .createFlags = 0,
    .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
    .vulkanPhysicalDevice = d->physical_device,
    .vulkanCreateInfo = &device_info,
    .vulkanAllocator = NULL,
  };


  VkResult vk_result;
  res = fun(self->instance, &info, &d->device, &vk_result);

  if (!xr_result(res, "Failed to create Vulkan graphics device."))
    return false;

  if (vk_result != VK_SUCCESS) {
    xrg_log_e("Runtime failed to create Vulkan device: %d\n", vk_result);
    return false;
  }

  return true;
}

static bool
_create_session(xr_example* self)
{
  XrSessionCreateInfoOverlayEXTX overlay_info = {
    .type = XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX,
    .next = &self->graphics_binding,
    .sessionLayersPlacement = 1,
  };

  void* session_next = self->extensions.overlay
                         ? (void*)&overlay_info
                         : (void*)&self->graphics_binding;

  XrSessionCreateInfo session_create_info = {
    .type = XR_TYPE_SESSION_CREATE_INFO,
    .next = session_next,
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
  proj->swapchains =
    (XrSwapchain*)malloc(sizeof(XrSwapchain) * self->view_count);

  proj->swapchain_length =
    (uint32_t*)malloc(sizeof(uint32_t) * self->view_count);

  proj->last_acquired = (uint32_t*)malloc(sizeof(uint32_t) * self->view_count);

  self->swapchain_format = swapchainFormats[0];

  for (uint32_t i = 0; i < self->view_count; i++) {
    XrSwapchainCreateInfo swapchainCreateInfo = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .createFlags = 0,
      .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
      // just use the first enumerated format
      .format = swapchainFormats[0],
      .sampleCount = 1,
      .width = self->configuration_views[i].recommendedImageRectWidth,
      .height = self->configuration_views[i].recommendedImageRectHeight,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = 1,
    };

    xrg_log_i("Swapchain %d dimensions: %dx%d", i,
              self->configuration_views[i].recommendedImageRectWidth,
              self->configuration_views[i].recommendedImageRectHeight);

    result = xrCreateSwapchain(self->session, &swapchainCreateInfo,
                               &proj->swapchains[i]);
    if (!xr_result(result, "Failed to create swapchain %d!", i))
      return false;

    result = xrEnumerateSwapchainImages(proj->swapchains[i], 0,
                                        &proj->swapchain_length[i], NULL);
    if (!xr_result(result, "Failed to enumerate swapchain lengths"))
      return false;
  }

  proj->images = (XrSwapchainImageVulkanKHR**)malloc(
    sizeof(XrSwapchainImageVulkanKHR*) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; i++) {
    proj->images[i] = (XrSwapchainImageVulkanKHR*)malloc(
      sizeof(XrSwapchainImageVulkanKHR) * proj->swapchain_length[i]);

    for (uint32_t j = 0; j < proj->swapchain_length[i]; j++) {
      // XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR aliased to
      // XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR
      proj->images[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
      proj->images[i][j].next = NULL;
    }
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


static bool
_create_depth_swapchains(xr_example* self, xr_proj* proj)
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
  proj->depth_swapchains =
    (XrSwapchain*)malloc(sizeof(XrSwapchain) * self->view_count);

  proj->depth_swapchain_length =
    (uint32_t*)malloc(sizeof(uint32_t) * self->view_count);

  proj->depth_last_acquired =
    (uint32_t*)malloc(sizeof(uint32_t) * self->view_count);

  self->depth_swapchain_format = 0;

  int64_t preference1 = VK_FORMAT_D32_SFLOAT;
  int64_t preference2 = VK_FORMAT_D16_UNORM;

  for (uint32_t i = 0; i < swapchainFormatCount; i++) {
    if (swapchainFormats[i] == preference1) {
      self->depth_swapchain_format = preference1;
    } else if (swapchainFormats[i] == preference2) {
      if (self->depth_swapchain_format == 0) {
        self->depth_swapchain_format = preference2;
      }
    }
  }

  if (self->depth_swapchain_format == 0) {
    xrg_log_e("None of our preferred depth swapchain formats are supported");
    return false;
  }
  xrg_log_i("Using depth swapchain format 0x%x", self->depth_swapchain_format);

  for (uint32_t i = 0; i < self->view_count; i++) {
    XrSwapchainCreateInfo swapchainCreateInfo = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .createFlags = 0,
      .usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      // just use the first enumerated format
      .format = self->depth_swapchain_format,
      .sampleCount = 1,
      .width = self->configuration_views[i].recommendedImageRectWidth,
      .height = self->configuration_views[i].recommendedImageRectHeight,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = 1,
    };

    xrg_log_i("depth Swapchain %d dimensions: %dx%d", i,
              self->configuration_views[i].recommendedImageRectWidth,
              self->configuration_views[i].recommendedImageRectHeight);

    result = xrCreateSwapchain(self->session, &swapchainCreateInfo,
                               &proj->depth_swapchains[i]);
    if (!xr_result(result, "Failed to create depth swapchain %d!", i))
      return false;

    result = xrEnumerateSwapchainImages(proj->depth_swapchains[i], 0,
                                        &proj->depth_swapchain_length[i], NULL);
    if (!xr_result(result, "Failed to enumerate depth swapchain lengths"))
      return false;
  }

  proj->depth_images = (XrSwapchainImageVulkanKHR**)malloc(
    sizeof(XrSwapchainImageVulkanKHR*) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; i++) {
    proj->depth_images[i] = (XrSwapchainImageVulkanKHR*)malloc(
      sizeof(XrSwapchainImageVulkanKHR) * proj->depth_swapchain_length[i]);

    for (uint32_t j = 0; j < proj->depth_swapchain_length[i]; j++) {
      // XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR aliased to
      // XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR
      proj->depth_images[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
      proj->depth_images[i][j].next = NULL;
    }
  }

  for (uint32_t i = 0; i < self->view_count; i++) {
    result = xrEnumerateSwapchainImages(
      proj->depth_swapchains[i], proj->depth_swapchain_length[i],
      &proj->depth_swapchain_length[i],
      (XrSwapchainImageBaseHeader*)proj->depth_images[i]);
    if (!xr_result(result, "Failed to enumerate depth swapchains"))
      return false;
    xrg_log_d("xrEnumerateSwapchainImages: depth swapchain_length[%d] %d", i,
              proj->depth_swapchain_length[i]);
  }

  return true;
}


static void
_create_projection_views(xr_example* self, xr_proj* proj)
{
  proj->views = (XrCompositionLayerProjectionView*)malloc(
    sizeof(XrCompositionLayerProjectionView) * self->view_count);

  for (uint32_t i = 0; i < self->view_count; i++) {
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

    if (proj->has_depth) {
      proj->depth_layer.subImage = (XrSwapchainSubImage){
        .swapchain = proj->depth_swapchains[i],
        .imageRect = {
          .extent = {
            .width = (int32_t) self->configuration_views[i].recommendedImageRectWidth,
            .height = (int32_t) self->configuration_views[i].recommendedImageRectHeight,
          },
        }
      };
    }
  }
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
    case XR_TYPE_EVENT_DATA_MAIN_SESSION_VISIBILITY_CHANGED_EXTX: {
      XrEventDataMainSessionVisibilityChangedEXTX* event =
        (XrEventDataMainSessionVisibilityChangedEXTX*)&runtimeEvent;
      self->main_session_visible = event->visible;
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
    .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
    .displayTime = self->frameState.predictedDisplayTime,
    .space = self->local_space,
  };

  self->views = (XrView*)malloc(sizeof(XrView) * self->view_count);
  for (uint32_t i = 0; i < self->view_count; i++) {

    self->views[i] = (XrView){ .type = XR_TYPE_VIEW };
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
xr_proj_acquire_swapchain(xr_example* self, xr_proj* proj, uint32_t i)
{
  (void)self;

  XrResult result;

  XrSwapchainImageAcquireInfo acquire_info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };

  result = xrAcquireSwapchainImage(proj->swapchains[i], &acquire_info,
                                   &proj->last_acquired[i]);
  if (!xr_result(result, "failed to acquire swapchain image!"))
    return false;

  XrSwapchainImageWaitInfo wait_info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
    .timeout = INT64_MAX,
  };
  result = xrWaitSwapchainImage(proj->swapchains[i], &wait_info);
  if (!xr_result(result, "failed to wait for swapchain image!"))
    return false;


  if (proj->has_depth) {
    result = xrAcquireSwapchainImage(proj->depth_swapchains[i], &acquire_info,
                                     &proj->depth_last_acquired[i]);
    if (!xr_result(result, "failed to acquire depth swapchain image!"))
      return false;

    result = xrWaitSwapchainImage(proj->depth_swapchains[i], &wait_info);
    if (!xr_result(result, "failed to wait for depth swapchain image!"))
      return false;
  }

  return true;
}

bool
xr_proj_release_swapchain(xr_example* self, xr_proj* proj, uint32_t i)
{
  (void)self;

  XrSwapchainImageReleaseInfo info = {
    .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  XrResult res = xrReleaseSwapchainImage(proj->swapchains[i], &info);
  if (!xr_result(res, "failed to release swapchain image!"))
    return false;

  if (proj->has_depth) {
    XrResult res = xrReleaseSwapchainImage(proj->depth_swapchains[i], &info);
    if (!xr_result(res, "failed to release depth swapchain image!"))
      return false;
  }

  return true;
}

static void
_init_layers(xr_example* self) {
  // sky layer is always "available" to be rendered
  self->num_layers = 1;

  if (!self->settings->disable_gears) {
    self->num_layers += 1;
  }

  if (!self->settings->disable_quad) {
    self->num_layers += 2;
  }

  self->layers = malloc(sizeof(const XrCompositionLayerBaseHeader*) * self->num_layers);
}

static void
_select_layers(xr_example* self)
{
  self->num_layers = 0;

  // if a main session is visible, don't occlude it with skybox
  if (!self->main_session_visible) {
    switch (self->sky_type) {
    case SKY_TYPE_PROJECTION:
      self->layers[self->num_layers++] =
        (const XrCompositionLayerBaseHeader* const)&self->sky.layer;
      break;
    case SKY_TYPE_EQUIRECT1:
      self->layers[self->num_layers++] =
        (const XrCompositionLayerBaseHeader* const)&self->equirect.layer_v1;
      break;
    case SKY_TYPE_EQUIRECT2:
      self->layers[self->num_layers++] =
        (const XrCompositionLayerBaseHeader* const)&self->equirect.layer_v2;
      break;
    default: break;
    }
  }

  if (!self->settings->disable_gears) {
    self->layers[self->num_layers++] =
      (const XrCompositionLayerBaseHeader* const)&self->gears.layer;

    for (uint32_t i = 0; i < self->view_count; i++) {
      self->gears.views[i].pose = self->views[i].pose;
      self->gears.views[i].fov = self->views[i].fov;

      if (self->gears.has_depth) {
        self->gears.views[i].next = &self->gears.depth_layer;

        self->gears.depth_layer.nearZ = self->near_z;
        self->gears.depth_layer.farZ = self->far_z;

        self->gears.depth_layer.minDepth = 0.0;
        self->gears.depth_layer.maxDepth = 1.0;
      }
    }
  }

  if (!self->settings->disable_quad) {
    self->layers[self->num_layers++] =
      (const XrCompositionLayerBaseHeader* const)&self->quad.layer;
    self->layers[self->num_layers++] =
      (const XrCompositionLayerBaseHeader* const)&self->quad2.layer;
  }
}

bool
xr_end_frame(xr_example* self)
{
  _select_layers(self);

  XrResult result;
  XrFrameEndInfo frameEndInfo = {
    .type = XR_TYPE_FRAME_END_INFO,
    .displayTime = self->frameState.predictedDisplayTime,
    .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    .layerCount = self->num_layers,
    .layers = self->layers,
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
  if (!self->settings->disable_gears) {
    _cleanup_proj(self, &self->gears);
  }

  if (self->sky_type == SKY_TYPE_PROJECTION)
    _cleanup_proj(self, &self->sky);

  xrDestroySpace(self->local_space);
  xrDestroySession(self->session);
  xrDestroyInstance(self->instance);

  free(self->layers);
}

static bool
_init_proj(xr_example* self,
           XrCompositionLayerFlags flags,
           xr_proj* proj,
           bool has_depth)
{
  proj->has_depth = has_depth;

  if (!_create_swapchains(self, proj))
    return false;

  if (has_depth) {
    if (!_create_depth_swapchains(self, proj))
      return false;
  }

  // has to be initialized before _create_projection_views
  proj->depth_layer = (XrCompositionLayerDepthInfoKHR){
    .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
  };

  _create_projection_views(self, proj);
  proj->layer = (XrCompositionLayerProjection){
    .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
    .layerFlags = flags,
    .space = self->local_space,
    .viewCount = self->view_count,
    .views = proj->views,
  };

  return true;
}

static bool
_init_vulkan_enable(xr_example* self,
                    VkInstance instance,
                    VkPhysicalDevice* physical_device)
{
  if (!_check_graphics_api_support(self))
    return false;

  if (!_get_vk_instance_extensions(self))
    return false;

  if (!_init_vk_device(self, instance, physical_device))
    return false;

  return true;
}

static bool
_init_vulkan_enable2(xr_example* self,
                     VkInstance* instance,
                     vulkan_device** vulkan_device)
{
  if (!_check_graphics_api_support2(self))
    return false;

  if (!_create_vk_instance2(self, instance))
    return false;

  VkPhysicalDevice physical_device;

  if (!_get_vk_device2(self, *instance, &physical_device))
    return false;

  if (!_create_vk_device2(self, physical_device, vulkan_device))
    return false;

  return true;
}

static bool
xr_init_pre_vk(xr_example* self, char* vulkan_extension)
{
  self->is_visible = true;
  self->is_runnting = true;
  self->main_session_visible = false;

  if (!_check_xr_extensions(self, vulkan_extension))
    return false;

  if (self->settings->disable_sky) {
    self->sky_type = SKY_TYPE_OFF;
  }

  if (!_enumerate_api_layers())
    return false;

  if (!_create_instance(self, vulkan_extension))
    return false;

  if (!_create_system(self))
    return false;

  if (!_set_up_views(self))
    return false;

  return true;
}

bool
xr_init(xr_example* self,
        VkInstance instance,
        VkPhysicalDevice* physical_device)
{
  if (!xr_init_pre_vk(self, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME))
    return false;

  if (!_init_vulkan_enable(self, instance, physical_device))
    return false;

  return true;
}

bool
xr_init2(xr_example* self, VkInstance* instance, vulkan_device** vulkan_device)
{
  if (!xr_init_pre_vk(self, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    return false;

  if (!_init_vulkan_enable2(self, instance, vulkan_device))
    return false;

  return true;
}

bool
xr_init_post_vk(xr_example* self,
                VkInstance instance,
                VkPhysicalDevice physical_device,
                VkDevice device,
                uint32_t queue_family_index,
                uint32_t queue_index)
{
  _init_layers(self);

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

  if (!self->settings->disable_gears) {
    _init_proj(self, XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
               &self->gears, true);
  }

  if (self->sky_type == SKY_TYPE_PROJECTION)
    _init_proj(self, XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT, &self->sky,
               false);

  return true;
}

#ifdef XR_OS_ANDROID
bool
xr_init_android(xr_example* self, struct android_app *app) {
  // Initialize the loader for this platform
  PFN_xrInitializeLoaderKHR initializeLoader = NULL;

  XrResult res = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                       (PFN_xrVoidFunction *)(&initializeLoader));

  if (!XR_SUCCEEDED(res))
    return false;

  XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
  memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
  loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
  loaderInitInfoAndroid.next = NULL;
  loaderInitInfoAndroid.applicationVM = app->activity->vm;
  loaderInitInfoAndroid.applicationContext = app->activity->clazz;
  res = initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *)&loaderInitInfoAndroid);

  if (!xr_result(res, "Failed to initialize Android loader."))
    return false;

  self->instanceCreateInfoAndroid = (XrInstanceCreateInfoAndroidKHR) {
    .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
    .applicationVM = app->activity->vm,
    .applicationActivity = app->activity->clazz,
  };

  return true;
}
#endif
