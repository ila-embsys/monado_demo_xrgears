/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <csignal>
#include <vector>
#include <string>

#include "gear.hpp"
#include "vulkan_framebuffer.hpp"
#include "log.h"
#include "xr.h"
#include "pipeline_equirect.hpp"
#include "pipeline_gears.hpp"
#include "glm_inc.hpp"
#include "settings.hpp"
#include "vulkan_instance.hpp"

#include "cat.ktx.h"
#include "hawk.ktx.h"

#define check_feature(f)                                                       \
  {                                                                            \
    if (device_features.f) {                                                   \
      enabled_features.f = VK_TRUE;                                            \
    } else {                                                                   \
      xrg_log_f("Feature not supported: %s", #f);                              \
    }                                                                          \
  }

class xrgears
{
public:
  bool quit = false;
  float animation_timer = 0.0;
  float revolutions_per_second = 0.0625;

  Settings settings;

  xr_example xr;

  vulkan_instance *instance;
  vulkan_device *vk_device;
  vulkan_framebuffer *offscreen_passes[2];

  struct
  {
    vulkan_buffer camera[2];
  } uniform_buffers;

  pipeline_gears *gears;
  pipeline_equirect *equirect;

  VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;

  VkDevice device;
  VkPhysicalDevice physical_device;
  VkCommandPool cmd_pool;
  VkQueue queue;
  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceFeatures enabled_features{};
  VkPipelineCache pipeline_cache;

  vulkan_texture quad_texture[3];

  struct
  {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec4 position;
  } ubo_camera[2];

  xrgears(int argc, char *argv[])
  {
    if (!settings.parse_args(argc, argv))
      xrg_log_f("Invalid arguments.");

    instance = new vulkan_instance(&settings);
  }

  ~xrgears()
  {
    for (uint32_t i = 0; i < 2; i++)
      if (offscreen_passes[i])
        delete offscreen_passes[i];

    delete gears;
    delete equirect;

    xr_cleanup(&xr);

    for (uint32_t i = 0; i < 2; i++)
      uniform_buffers.camera[i].destroy();

    vkDestroyPipelineCache(device, pipeline_cache, nullptr);

    vkDestroyCommandPool(device, cmd_pool, nullptr);

    delete vk_device;
    delete instance;

    xrg_log_d("Shut down xrgears");
  }

  void
  loop()
  {
    while (!quit)
      render();
    vkDeviceWaitIdle(device);
  }

  // Enable physical device features required for this example
  void
  check_required_features()
  {
    check_feature(samplerAnisotropy);
    check_feature(textureCompressionBC);
  }

  void
  build_command_buffer()
  {
    if (cmd_buffer == VK_NULL_HANDLE)
      cmd_buffer = create_command_buffer();

    VkCommandBufferBeginInfo command_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vk_check(vkBeginCommandBuffer(cmd_buffer, &command_buffer_info));

    for (uint32_t i = 0; i < 2; i++) {
      offscreen_passes[i]->begin_render_pass(cmd_buffer);
      offscreen_passes[i]->set_viewport_and_scissor(cmd_buffer);

      equirect->draw(cmd_buffer, i);
      gears->draw(cmd_buffer, i);

      vkCmdEndRenderPass(cmd_buffer);
    }

    vk_check(vkEndCommandBuffer(cmd_buffer));
  }

  static inline void
  fix_handedness(glm::mat4 &m)
  {
    m[0][1] = -m[0][1];
    m[1][0] = -m[1][0];
    m[1][2] = -m[1][2];
    m[2][1] = -m[2][1];
  }

  static glm::mat4
  _create_projection_from_fov(const XrFovf fov,
                              const float near_z,
                              const float far_z)
  {
    const float tan_left = tanf(fov.angleLeft);
    const float tan_right = tanf(fov.angleRight);

    const float tan_down = tanf(fov.angleDown);
    const float tan_up = tanf(fov.angleUp);

    const float tan_width = tan_right - tan_left;
    const float tan_height = tan_up - tan_down;

    const float a11 = 2 / tan_width;
    const float a22 = 2 / tan_height;

    const float a31 = (tan_right + tan_left) / tan_width;
    const float a32 = (tan_up + tan_down) / tan_height;
    const float a33 = -far_z / (far_z - near_z);

    const float a43 = -(far_z * near_z) / (far_z - near_z);

    const float mat[16] = {
      a11, 0, 0, 0, 0, a22, 0, 0, a31, a32, a33, -1, 0, 0, a43, 0,
    };

    return glm::make_mat4(mat);
  }

  static glm::mat4
  _create_view_from_pose(XrPosef *pose)
  {
    glm::quat quat = glm::quat(pose->orientation.w, pose->orientation.x,
                               pose->orientation.y, pose->orientation.z);
    glm::mat4 rotation = glm::mat4_cast(quat);

    glm::vec3 position =
      glm::vec3(pose->position.x, pose->position.y, pose->position.z);

    glm::mat4 translation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 view_glm = translation * rotation;

    glm::mat4 view_glm_inv = glm::inverse(view_glm);

    fix_handedness(view_glm_inv);

    return view_glm_inv;
  }

  void
  draw()
  {
    xr_begin_frame(&xr);

    for (uint32_t i = 0; i < 2; i++) {
      uint32_t buffer_index;
      if (!xr_aquire_swapchain(&xr, i, &buffer_index)) {
        xrg_log_e("Could not aquire xr swapchain");
        return;
      }
      ubo_camera[i].projection =
        _create_projection_from_fov(xr.views[i].fov, 0.05f, 100.0f);
      ubo_camera[i].view = _create_view_from_pose(&xr.views[i].pose);

      ubo_camera[i].position =
        glm::vec4(xr.views[i].pose.position.x, xr.views[i].pose.position.y,
                  xr.views[i].pose.position.z, 1);
      memcpy(uniform_buffers.camera[i].mapped, &ubo_camera[i],
             sizeof(ubo_camera[i]));
    }


    gears->update_uniform_buffers(animation_timer);

    for (uint32_t i = 0; i < 2; i++)
      equirect->update_uniform_buffers(ubo_camera[i].projection,
                                       ubo_camera[i].view, i);

    VkPipelineStageFlags stage_flags[1] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = stage_flags,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd_buffer,
    };

    vk_check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

    for (uint32_t i = 0; i < 2; i++)
      if (!xr_release_swapchain(&xr, i)) {
        xrg_log_e("Could not release xr swapchain");
        return;
      }

    if (!xr_end_frame(&xr)) {
      xrg_log_e("Could not end xr frame");
    }
  }

  void
  init_quads()
  {
    float ppm = 1000;
    XrExtent2Di extent = { .width = 1080, .height = 1920 };
    XrPosef pose = {
      .orientation = { .x = 0, .y = 0, .z = 0, .w = 1 },
      .position = { .x = -2, .y = 1, .z = -3 },
    };
    XrExtent2Df size = {
      .width = extent.width / ppm,
      .height = extent.height / ppm,
    };
    xr_quad_init(&xr.quad, xr.session, xr.local_space, extent, pose, size);

    ktx_size_t tex_size = sizeof(hawk_ktx) / sizeof(hawk_ktx[0]);

    for (uint32_t i = 0; i < xr.quad.swapchain_length; i++) {
      uint32_t buffer_index;
      if (!xr_quad_acquire_swapchain(&xr.quad, &buffer_index))
        xrg_log_e("Could not acquire quad swapchain.");
      quad_texture[i].load_from_ktx(xr.quad.images[i].image, hawk_ktx, tex_size,
                                    vk_device, queue);
      if (!xr_quad_release_swapchain(&xr.quad))
        xrg_log_e("Could not release quad swapchain.");
    }

    XrExtent2Di extent2 = { .width = 2370, .height = 1570 };
    XrPosef pose2 = {
      .orientation = { .x = 0, .y = 0, .z = 0, .w = 1 },
      .position = { .x = 2, .y = 1, .z = -3 },
    };
    XrExtent2Df size2 = {
      .width = extent2.width / ppm,
      .height = extent2.height / ppm,
    };
    xr_quad_init(&xr.quad2, xr.session, xr.local_space, extent2, pose2, size2);

    ktx_size_t tex_size2 = sizeof(cat_ktx) / sizeof(cat_ktx[0]);

    for (uint32_t i = 0; i < xr.quad2.swapchain_length; i++) {
      uint32_t buffer_index;
      if (!xr_quad_acquire_swapchain(&xr.quad2, &buffer_index))
        xrg_log_e("Could not acquire quad swapchain.");
      quad_texture[i].load_from_ktx(xr.quad2.images[i].image, cat_ktx,
                                    tex_size2, vk_device, queue);
      if (!xr_quad_release_swapchain(&xr.quad2))
        xrg_log_e("Could not release quad swapchain.");
    }
  }

  bool
  init()
  {
    init_vulkan();
    create_pipeline_cache();
    create_command_pool(0);

    if (!xr_init(&xr, instance->instance, physical_device, device,
                 vk_device->queue_family_indices.graphics, 0)) {
      xrg_log_e("OpenXR initialization failed.");
      return false;
    }

    for (uint32_t i = 0; i < 2; i++) {
      offscreen_passes[i] = new vulkan_framebuffer(device);
      offscreen_passes[i]->init_offscreen_framebuffer(
        vk_device, xr.images[i][0].image, (VkFormat)xr.swapchain_format,
        xr.configuration_views[i].recommendedImageRectWidth,
        xr.configuration_views[i].recommendedImageRectHeight);
    }
    xrg_log_i("Initialized OpenXR with %d views.", xr.view_count);

    for (uint32_t i = 0; i < 2; i++)
      vk_device->create_and_map(&uniform_buffers.camera[i],
                                sizeof(ubo_camera[i]));

    VkDescriptorBufferInfo *camera_descriptors[2] = {
      &uniform_buffers.camera[0].descriptor,
      &uniform_buffers.camera[1].descriptor
    };

    gears = new pipeline_gears(vk_device, offscreen_passes[0]->render_pass,
                               pipeline_cache, camera_descriptors);

    equirect = new pipeline_equirect(
      vk_device, queue, offscreen_passes[0]->render_pass, pipeline_cache);

    build_command_buffer();

    init_quads();

    return true;
  }

  void
  exit()
  {
    quit = true;
  }

  VkCommandBuffer
  create_command_buffer()
  {
    VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };

    VkCommandBuffer cmd_buffer;
    vk_check(vkAllocateCommandBuffers(device, &info, &cmd_buffer));

    return cmd_buffer;
  }

  void
  create_pipeline_cache()
  {
    VkPipelineCacheCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    vk_check(vkCreatePipelineCache(device, &info, nullptr, &pipeline_cache));
  }

  void
  init_physical_device()
  {
    VkResult err;

    // Physical device
    uint32_t gpu_count = 0;
    // Get number of available physical devices
    vk_check(
      vkEnumeratePhysicalDevices(instance->instance, &gpu_count, nullptr));
    assert(gpu_count > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpu_count);
    err = vkEnumeratePhysicalDevices(instance->instance, &gpu_count,
                                     physicalDevices.data());

    xrg_log_f_if(err, "Could not enumerate physical devices: %s",
                 vk_result_to_string(err));

    // Select first device by default
    if (settings.gpu == -1)
      settings.gpu = 0;

    // Select physical device to be used for the Vulkan example
    // Defaults to the first device unless specified by command line
    uint32_t selected_device = 0;
    if (settings.gpu > (int)gpu_count - 1) {
      xrg_log_f(
        "Selected device index %d is out of range,"
        " reverting to device 0",
        settings.gpu);
    } else if (settings.gpu != 0) {
      xrg_log_i("Selected Vulkan device %d", settings.gpu);
      selected_device = settings.gpu;
    }

    physical_device = physicalDevices[selected_device];
  }

  void
  init_vulkan()
  {
    VkResult err = instance->create_instance();
    xrg_log_f_if(err, "Could not create Vulkan instance: %s",
                 vk_result_to_string(err));

    init_physical_device();

    vkGetPhysicalDeviceFeatures(physical_device, &device_features);

    check_required_features();

    vk_device = new vulkan_device(physical_device);

    VkResult res = vk_device->create_device(enabled_features);
    xrg_log_f_if(res != VK_SUCCESS, "Could not create Vulkan device: %s",
                 vk_result_to_string(res));

    device = vk_device->device;

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vk_device->queue_family_indices.graphics, 0,
                     &queue);
  }

  void
  create_command_pool(uint32_t index)
  {
    VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = index
    };

    vk_check(vkCreateCommandPool(device, &cmd_pool_info, nullptr, &cmd_pool));
  }

  void
  update_timer()
  {
    const float sec_in_nsec = 1000000000.0f;

    struct timespec mono_time;
    if (clock_gettime(CLOCK_MONOTONIC, &mono_time) != 0) {
      xrg_log_e("Could not read system clock");
      return;
    }

    float mono_secs =
      ((float)mono_time.tv_sec + (mono_time.tv_nsec / sec_in_nsec));

    animation_timer = revolutions_per_second * mono_secs;
  }

  void
  render()
  {
    vkDeviceWaitIdle(device);
    draw();
    update_timer();
  }
};

static xrgears *app;
static void
sigint_cb(int signum)
{
  (void)signum;
  app->exit();
}

int
main(int argc, char *argv[])
{
  app = new xrgears(argc, argv);
  if (!app->init())
    return -1;

  signal(SIGINT, sigint_cb);

  app->loop();
  delete app;

  return 0;
}
