/*
 * xrgears
 *
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <stdbool.h>

#include "vulkan/vulkan.h"

#define xrg_log(...) xrg_log_full(__FILE__, __LINE__, __VA_ARGS__)
#define xrg_log_d(...) xrg_log(LOG_DEBUG, __VA_ARGS__)
#define xrg_log_i(...) xrg_log(LOG_INFO, __VA_ARGS__)
#define xrg_log_w(...) xrg_log(LOG_WARNING, __VA_ARGS__)
#define xrg_log_e(...) xrg_log(LOG_ERROR, __VA_ARGS__)
#define xrg_log_f(...) xrg_log(LOG_FATAL, __VA_ARGS__)
#define xrg_log_if(...) xrg_log_full_if(__FILE__, __LINE__, __VA_ARGS__)
#define xrg_log_f_if(...) xrg_log_if(LOG_FATAL, __VA_ARGS__)
#define xrg_log_e_if(...) xrg_log_if(LOG_ERROR, __VA_ARGS__)

// Macro to check and display Vulkan return results
#define vk_check(f)                                                            \
  {                                                                            \
    VkResult res = (f);                                                        \
    xrg_log_f_if(res != VK_SUCCESS, "VkResult: %s", vk_result_to_string(res)); \
  }

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  LOG_DEBUG = 0,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_FATAL
} xrg_log_type;

const char *
vk_result_to_string(VkResult code);

const char *
xrg_log_type_str(xrg_log_type t);

int
xrg_log_type_color(xrg_log_type t);

FILE *
xrg_log_type_stream(xrg_log_type t);

void
xrg_log_values(
  const char *file, int line, xrg_log_type t, const char *format, va_list args);

void
xrg_log_full(
  const char *file, int line, xrg_log_type t, const char *format, ...);

void
xrg_log_full_if(const char *file,
                int line,
                xrg_log_type t,
                bool cond,
                const char *format,
                ...);

#ifdef __cplusplus
}
#endif
