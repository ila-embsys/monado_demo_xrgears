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

#include <string>
#include <cassert>
#include <fstream>
#include <cerrno>

#include "log.h"

class vulkan_shader
{
public:
  static VkPipelineShaderStageCreateInfo
  load(VkDevice device,
       const uint32_t* code,
       size_t size,
       VkShaderStageFlagBits stage)
  {
    VkShaderModule module;
    VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = code
    };

    vk_check(vkCreateShaderModule(device, &info, NULL, &module));

    return (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = stage,
      .module = module,
      .pName = "main"
    };
  }
};
