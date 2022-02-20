/*
 * xrgears
 *
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int gpu;
  bool vulkan_enable2;
  bool enable_sky;
  bool enable_quad;
  bool enable_gears;
  bool enable_overlay;
} xrg_settings;

bool
settings_parse_args(xrg_settings *self, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
