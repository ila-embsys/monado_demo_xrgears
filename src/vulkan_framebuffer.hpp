/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <array>

#include "vulkan_device.hpp"

class vulkan_framebuffer
{
public:
  VkDevice device;

  VkImageView color_view;

  struct
  {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
  } depth;

  uint32_t width, height;
  VkFramebuffer frame_buffer;
  VkRenderPass render_pass;


public:
  explicit vulkan_framebuffer(const VkDevice& d)
  {
    device = d;
  }

  ~vulkan_framebuffer()
  {
    // Color attachments
    vkDestroyImageView(device, color_view, nullptr);

    // Depth attachment
    vkDestroyImageView(device, depth.view, nullptr);
    vkDestroyImage(device, depth.image, nullptr);
    vkFreeMemory(device, depth.mem, nullptr);

    vkDestroyFramebuffer(device, frame_buffer, nullptr);

    vkDestroyRenderPass(device, render_pass, nullptr);
  }

  void
  create_depth_attachment(vulkan_device* vulkanDevice, VkFormat format)
  {
    depth.format = format;

    VkImageCreateInfo image = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = { .width = width, .height = height, .depth = 1 },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
    };
    vk_check(vkCreateImage(device, &image, nullptr, &depth.image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, depth.image, &mem_reqs);

    VkMemoryAllocateInfo mem_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex = vulkanDevice->get_memory_type(
        mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    vk_check(vkAllocateMemory(device, &mem_alloc, nullptr, &depth.mem));
    vk_check(vkBindImageMemory(device, depth.image, depth.mem, 0));

    VkImageViewCreateInfo image_view = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = depth.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange =
      {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vk_check(vkCreateImageView(device, &image_view, nullptr, &depth.view));
  }

  void
  init_offscreen_framebuffer(vulkan_device* vulkanDevice,
                             VkImage color_image,
                             VkFormat color_format,
                             uint32_t width,
                             uint32_t height)
  {
    this->width = width;
    this->height = height;

    create_depth_attachment(vulkanDevice, VK_FORMAT_D32_SFLOAT);

    // Set up separate renderpass with references to the color and depth
    // attachments
    std::array<VkAttachmentDescription, 2> attachmentDescs = {};

    // Init attachment properties
    for (uint32_t i = 0; i < 2; ++i) {
      attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      if (i == 1) {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
    }

    // Formats
    attachmentDescs[0].format = color_format;
    attachmentDescs[1].format = depth.format;

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(colorReferences.size()),
      .pColorAttachments = colorReferences.data(),
      .pDepthStencilAttachment = &depthReference
    };

    // Use subpass dependencies for attachment layput transitions
    std::array<VkSubpassDependency, 2> dependencies = {
      (VkSubpassDependency){
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT },
      (VkSubpassDependency){
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT }
    };

    VkRenderPassCreateInfo renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = static_cast<uint32_t>(attachmentDescs.size()),
      .pAttachments = attachmentDescs.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 2,
      .pDependencies = dependencies.data()
    };

    vk_check(
      vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass));

    VkImageViewCreateInfo imageView = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = color_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = color_format,
      .subresourceRange =
      {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vk_check(vkCreateImageView(device, &imageView, nullptr, &color_view));

    std::array<VkImageView, 2> attachments;
    attachments[0] = color_view;
    attachments[1] = depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = render_pass,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments = attachments.data(),
      .width = width,
      .height = height,
      .layers = 1
    };

    vk_check(
      vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frame_buffer));
  }

  void
  begin_render_pass(const VkCommandBuffer& cmdBuffer)
  {
    // Clear values for all attachments written in the fragment sahder
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = render_pass,
      .framebuffer = frame_buffer,
      .renderArea = { .extent = { .width = width, .height = height } },
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()
    };

    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
  }

  void
  set_viewport_and_scissor(const VkCommandBuffer& cmdBuffer)
  {
    VkViewport viewport = { .width = (float)width,
                            .height = (float)height,
                            .minDepth = 0.0f,
                            .maxDepth = 1.0f };
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = { .offset = { .x = 0, .y = 0 },
                         .extent = { .width = width, .height = height } };
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
  }
};
