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
#include "vulkan_framebuffer.h"
#include "log.h"
#include "xr.h"
#include "pipeline_equirect.hpp"
#include "pipeline_gears.hpp"
#include "glm_inc.hpp"
#include "settings.h"
#include "vulkan_context.h"

#include "textures.h"

class xrgears
{
public:
  bool quit = false;
  float animation_timer = 0.0;
  float revolutions_per_second = 0.0625;

  xrg_settings settings;

  xr_example xr;

  vulkan_context context;
  vulkan_device *vk_device;

#if ENABLE_GEARS_LAYER
  vulkan_pipeline *gears;
  vulkan_framebuffer **gears_buffers[2];
  VkCommandBuffer *gears_draw_cmd;
#endif

#if ENABLE_SKY_LAYER
  vulkan_pipeline *equirect;
  vulkan_framebuffer **sky_buffers[2];
  VkCommandBuffer *sky_draw_cmd;
#endif

  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkCommandPool cmd_pool;
  VkQueue queue;
  VkPhysicalDeviceFeatures device_features;
  VkPipelineCache pipeline_cache;

#if ENABLE_QUAD_LAYERS
  vulkan_texture quad_texture[3];
#endif

  xrgears(int argc, char *argv[])
  {
    if (!settings_parse_args(&settings, argc, argv))
      xrg_log_f("Invalid arguments.");
  }

  ~xrgears()
  {
#if ENABLE_GEARS_LAYER
    for (uint32_t i = 0; i < 2; i++) {
      for (uint32_t j = 0; j < xr.gears.swapchain_length[i]; j++)
        if (gears_buffers[i][j]) {
          vulkan_framebuffer_destroy(gears_buffers[i][j]);
          delete gears_buffers[i][j];
        }
      free(gears_buffers[i]);
    }
    free(gears_draw_cmd);
    delete gears;
#endif

#if ENABLE_SKY_LAYER
    for (uint32_t i = 0; i < 2; i++) {
      for (uint32_t j = 0; j < xr.sky.swapchain_length[i]; j++)
        if (sky_buffers[i][j]) {
          vulkan_framebuffer_destroy(sky_buffers[i][j]);
          delete sky_buffers[i][j];
        }
      free(sky_buffers[i]);
    }
    free(sky_draw_cmd);
    delete equirect;
#endif

    xr_cleanup(&xr);

    vkDestroyPipelineCache(device, pipeline_cache, nullptr);

    vkDestroyCommandPool(device, cmd_pool, nullptr);

    vulkan_device_destroy(vk_device);
    vulkan_context_destroy(&context);

    xrg_log_d("Shut down xrgears");
  }

  void
  loop()
  {
    while (!quit)
      render();
    vkDeviceWaitIdle(device);
  }

  void
  build_command_buffer(VkCommandBuffer *cb,
                       vulkan_framebuffer ***fb,
                       uint32_t view_count,
                       uint32_t swapchain_index,
                       vulkan_pipeline *pipe)
  {
    *cb = create_command_buffer();

    VkCommandBufferBeginInfo command_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vk_check(vkBeginCommandBuffer(*cb, &command_buffer_info));

    for (uint32_t view_index = 0; view_index < view_count; view_index++) {
      vulkan_framebuffer_begin_render_pass(fb[view_index][swapchain_index],
                                           *cb);
      vulkan_framebuffer_set_viewport_and_scissor(
        fb[view_index][swapchain_index], *cb);

      pipe->draw(*cb, view_index);

      vkCmdEndRenderPass(*cb);
    }

    vk_check(vkEndCommandBuffer(*cb));
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

    uint32_t buffer_index;
    for (uint32_t i = 0; i < 2; i++) {
#if ENABLE_GEARS_LAYER
      if (!xr_aquire_swapchain(&xr, &xr.gears, i, &buffer_index)) {
        xrg_log_e("Could not aquire xr swapchain");
        return;
      }
#endif

#if ENABLE_SKY_LAYER
      if (!xr_aquire_swapchain(&xr, &xr.sky, i, &buffer_index)) {
        xrg_log_e("Could not aquire xr swapchain");
        return;
      }
#endif

      glm::mat4 projection =
        _create_projection_from_fov(xr.views[i].fov, 0.05f, 100.0f);
      glm::mat4 view = _create_view_from_pose(&xr.views[i].pose);

#if ENABLE_GEARS_LAYER
      ((pipeline_gears *)gears)->update_vp(projection, view, i);
#endif

#if ENABLE_SKY_LAYER
      ((pipeline_equirect *)equirect)->update_vp(projection, view, i);
#endif
    }

#if ENABLE_GEARS_LAYER
    ((pipeline_gears *)gears)->update_time(animation_timer);
#endif

    VkPipelineStageFlags stage_flags[1] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = stage_flags,
      .commandBufferCount = 1,
    };

#if ENABLE_GEARS_LAYER
    submit_info.pCommandBuffers = &gears_draw_cmd[buffer_index];
    vk_check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
#endif

#if ENABLE_SKY_LAYER
    submit_info.pCommandBuffers = &sky_draw_cmd[buffer_index];
    vk_check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
#endif

    for (uint32_t i = 0; i < 2; i++) {
#if ENABLE_GEARS_LAYER
      if (!xr_release_swapchain(xr.gears.swapchains[i])) {
        xrg_log_e("Could not release xr swapchain");
        return;
      }
#endif

#if ENABLE_SKY_LAYER
      if (!xr_release_swapchain(xr.sky.swapchains[i])) {
        xrg_log_e("Could not release xr swapchain");
        return;
      }
#endif
    }

    if (!xr_end_frame(&xr)) {
      xrg_log_e("Could not end xr frame");
    }
  }

#if ENABLE_QUAD_LAYERS
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

    for (uint32_t i = 0; i < xr.quad.swapchain_length; i++) {
      uint32_t buffer_index;
      if (!xr_quad_acquire_swapchain(&xr.quad, &buffer_index))
        xrg_log_e("Could not acquire quad swapchain.");
      vulkan_texture_load_ktx_from_image(
        &quad_texture[i], xr.quad.images[i].image, hawk_bytes(), hawk_size(),
        vk_device, queue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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

    for (uint32_t i = 0; i < xr.quad2.swapchain_length; i++) {
      uint32_t buffer_index;
      if (!xr_quad_acquire_swapchain(&xr.quad2, &buffer_index))
        xrg_log_e("Could not acquire quad swapchain.");
      vulkan_texture_load_ktx_from_image(
        &quad_texture[i], xr.quad2.images[i].image, cat_bytes(), cat_size(),
        vk_device, queue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      if (!xr_quad_release_swapchain(&xr.quad2))
        xrg_log_e("Could not release quad swapchain.");
    }
  }
#endif

  bool
  init()
  {
    init_vulkan();

    if (!xr_init(&xr, context.instance, &physical_device)) {
      xrg_log_e("OpenXR graphics initialization failed.");
      return false;
    }

    init_vulkan_device();
    create_pipeline_cache();
    create_command_pool(0);

    if (!xr_init_post_vk(&xr, context.instance, physical_device, device,
                         vk_device->graphics_family_index, 0)) {
      xrg_log_e("OpenXR initialization failed.");
      return false;
    }
    xrg_log_i("Initialized OpenXR with %d views.", xr.view_count);

    for (uint32_t i = 0; i < xr.view_count; i++) {

#if ENABLE_GEARS_LAYER
      gears_draw_cmd = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) *
                                                 xr.gears.swapchain_length[i]);
      gears_buffers[i] = (vulkan_framebuffer **)malloc(
        sizeof(vulkan_framebuffer *) * xr.gears.swapchain_length[i]);
      for (uint32_t j = 0; j < xr.gears.swapchain_length[i]; j++) {
        gears_buffers[i][j] = vulkan_framebuffer_create(device);
        vulkan_framebuffer_init(
          gears_buffers[i][j], vk_device, xr.gears.images[i][j].image,
          //(VkFormat)xr.swapchain_format,
          VK_FORMAT_R8G8B8A8_UNORM,
          xr.configuration_views[i].recommendedImageRectWidth,
          xr.configuration_views[i].recommendedImageRectHeight);
      }
#endif

#if ENABLE_SKY_LAYER
      sky_draw_cmd = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) *
                                               xr.sky.swapchain_length[i]);

      sky_buffers[i] = (vulkan_framebuffer **)malloc(
        sizeof(vulkan_framebuffer *) * xr.sky.swapchain_length[i]);
      for (uint32_t j = 0; j < xr.sky.swapchain_length[i]; j++) {
        sky_buffers[i][j] = vulkan_framebuffer_create(device);
        vulkan_framebuffer_init(
          sky_buffers[i][j], vk_device, xr.sky.images[i][j].image,
          (VkFormat)xr.swapchain_format,
          xr.configuration_views[i].recommendedImageRectWidth,
          xr.configuration_views[i].recommendedImageRectHeight);
      }
#endif
    }

#if ENABLE_GEARS_LAYER
    gears = new pipeline_gears(vk_device, gears_buffers[0][0]->render_pass,
                               pipeline_cache);
    for (uint32_t i = 0; i < xr.gears.swapchain_length[0]; i++)
      build_command_buffer(&gears_draw_cmd[i], gears_buffers, xr.view_count, i,
                           gears);
#endif

#if ENABLE_SKY_LAYER
    equirect = new pipeline_equirect(
      vk_device, queue, sky_buffers[0][0]->render_pass, pipeline_cache);
    for (uint32_t i = 0; i < xr.sky.swapchain_length[0]; i++)
      build_command_buffer(&sky_draw_cmd[i], sky_buffers, xr.view_count, i,
                           equirect);
#endif

#if ENABLE_QUAD_LAYERS
    init_quads();
#endif

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
  init_vulkan()
  {
    VkResult err = vulkan_context_create_instance(&context);
    xrg_log_f_if(err, "Could not create Vulkan instance: %s",
                 vk_result_to_string(err));
  }

  void
  init_vulkan_device()
  {
    vk_device = vulkan_device_create(physical_device);

    VkResult res = vulkan_device_create_device(vk_device);
    xrg_log_f_if(res != VK_SUCCESS, "Could not create Vulkan device: %s",
                 vk_result_to_string(res));

    device = vk_device->device;

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vk_device->graphics_family_index, 0, &queue);
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
