/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

VkPipelineShaderStageCreateInfo
vulkan_shader_load(VkDevice device,
                   const uint32_t* code,
                   size_t size,
                   VkShaderStageFlagBits stage);

#ifdef __cplusplus
}
#endif
