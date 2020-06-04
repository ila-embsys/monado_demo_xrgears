/*
 * xrgears
 *
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  VkInstance instance;
} vulkan_context;

void
vulkan_context_destroy(vulkan_context *self);

VkResult
vulkan_context_create_instance(vulkan_context *self);

#ifdef __cplusplus
}
#endif
