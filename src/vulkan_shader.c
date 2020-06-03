/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "log.h"

VkPipelineShaderStageCreateInfo
vulkan_shader_load(VkDevice device,
                   const uint32_t* code,
                   size_t size,
                   VkShaderStageFlagBits stage)
{
  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = size,
    .pCode = code
  };

  VkShaderModule module;
  vk_check(vkCreateShaderModule(device, &info, NULL, &module));

  return (VkPipelineShaderStageCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = stage,
    .module = module,
    .pName = "main"
  };
}
