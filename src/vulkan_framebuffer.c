/*
 * xrgears
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "vulkan_framebuffer.h"

vulkan_framebuffer*
vulkan_framebuffer_create(VkDevice d)
{
  vulkan_framebuffer* self = malloc(sizeof(vulkan_framebuffer));
  self->device = d;
  return self;
}

void
vulkan_framebuffer_destroy(vulkan_framebuffer* self)
{
  // Color attachments
  vkDestroyImageView(self->device, self->color_view, NULL);

  // Depth attachment
  vkDestroyImageView(self->device, self->depth.view, NULL);
  vkDestroyImage(self->device, self->depth.image, NULL);
  vkFreeMemory(self->device, self->depth.mem, NULL);

  vkDestroyFramebuffer(self->device, self->frame_buffer, NULL);

  vkDestroyRenderPass(self->device, self->render_pass, NULL);
}

static void
_create_depth_attachment(vulkan_framebuffer* self,
                         vulkan_device* vulkanDevice,
                         VkFormat format)
{
  self->depth.format = format;

  VkImageCreateInfo image = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = { .width = self->width, .height = self->height, .depth = 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };
  vk_check(vkCreateImage(self->device, &image, NULL, &self->depth.image));

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(self->device, self->depth.image, &mem_reqs);

  VkMemoryAllocateInfo mem_alloc = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_reqs.size,
  };

  if (!vulkan_device_get_memory_type(vulkanDevice, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &mem_alloc.memoryTypeIndex))
    xrg_log_e("Could not find memory type.");

  vk_check(vkAllocateMemory(self->device, &mem_alloc, NULL, &self->depth.mem));
  vk_check(
    vkBindImageMemory(self->device, self->depth.image, self->depth.mem, 0));

  VkImageViewCreateInfo image_view = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = self->depth.image,
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

  vk_check(
    vkCreateImageView(self->device, &image_view, NULL, &self->depth.view));
}

void
vulkan_framebuffer_init(vulkan_framebuffer* self,
                        vulkan_device* vulkanDevice,
                        VkImage color_image,
                        VkFormat color_format,
                        uint32_t width,
                        uint32_t height)
{
  self->width = width;
  self->height = height;

  _create_depth_attachment(self, vulkanDevice, VK_FORMAT_D32_SFLOAT);

  // Set up separate renderpass with references to the color and depth
  // attachments
  VkAttachmentDescription* attachmentDescs =
    malloc(sizeof(VkAttachmentDescription) * 2);

  // Init attachment properties
  for (uint32_t i = 0; i < 2; ++i) {
    attachmentDescs[i].flags = 0;
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
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
  }

  // Formats
  attachmentDescs[0].format = color_format;
  attachmentDescs[1].format = self->depth.format;

  VkAttachmentReference colorReferences[1] = {
    { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
  };

  VkAttachmentReference depthReference = {
    .attachment = 1,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = ARRAY_SIZE(colorReferences),
    .pColorAttachments = colorReferences,
    .pDepthStencilAttachment = &depthReference,
  };

  // Use subpass dependencies for attachment layput transitions
  VkSubpassDependency dependencies[2] = {
    (VkSubpassDependency){
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
    (VkSubpassDependency){
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    }
  };

  VkRenderPassCreateInfo renderPassInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 2,
    .pAttachments = attachmentDescs,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 2,
    .pDependencies = dependencies,
  };

  vk_check(vkCreateRenderPass(self->device, &renderPassInfo, NULL,
                              &self->render_pass));

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

  vk_check(
    vkCreateImageView(self->device, &imageView, NULL, &self->color_view));

  VkImageView attachments[2] = { self->color_view, self->depth.view };

  VkFramebufferCreateInfo fbufCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = self->render_pass,
    .attachmentCount = ARRAY_SIZE(attachments),
    .pAttachments = attachments,
    .width = width,
    .height = height,
    .layers = 1,
  };

  vk_check(vkCreateFramebuffer(self->device, &fbufCreateInfo, NULL,
                               &self->frame_buffer));
}

void
vulkan_framebuffer_begin_render_pass(vulkan_framebuffer* self,
                                     VkCommandBuffer cmdBuffer)
{
  // Clear values for all attachments written in the fragment sahder
  VkClearValue clearValues[2] = { { .color = { { 0.0f, 0.0f, 0.0f, 0.0f } } },
                                  { .depthStencil = { 1.0f, 0 } } };

  VkRenderPassBeginInfo renderPassBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = self->render_pass,
    .framebuffer = self->frame_buffer,
    .renderArea = { .extent = { .width = self->width,
                                .height = self->height } },
    .clearValueCount = ARRAY_SIZE(clearValues),
    .pClearValues = clearValues,
  };

  vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void
vulkan_framebuffer_set_viewport_and_scissor(vulkan_framebuffer* self,
                                            VkCommandBuffer cmdBuffer)
{
  VkViewport viewport = { .width = (float)self->width,
                          .height = (float)self->height,
                          .minDepth = 0.0f,
                          .maxDepth = 1.0f };
  vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

  VkRect2D scissor = { .offset = { .x = 0, .y = 0 },
                       .extent = { .width = self->width,
                                   .height = self->height } };
  vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}
