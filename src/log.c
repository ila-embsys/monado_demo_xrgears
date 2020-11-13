/*
 * xrgears
 *
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "log.h"

#ifdef XR_OS_ANDROID
#include <android/log.h>
#define USE_COLOR 0
#else
#define USE_COLOR 1
#endif

#define LOG_TO_STD_ERR 0
#define RESET_COLOR "\e[0m"

#define ENUM_TO_STR(r)                                                         \
  case r: return #r

const char *
vk_result_to_string(VkResult code)
{
  switch (code) {
    ENUM_TO_STR(VK_SUCCESS);
    ENUM_TO_STR(VK_NOT_READY);
    ENUM_TO_STR(VK_TIMEOUT);
    ENUM_TO_STR(VK_EVENT_SET);
    ENUM_TO_STR(VK_EVENT_RESET);
    ENUM_TO_STR(VK_INCOMPLETE);
    ENUM_TO_STR(VK_ERROR_OUT_OF_HOST_MEMORY);
    ENUM_TO_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    ENUM_TO_STR(VK_ERROR_INITIALIZATION_FAILED);
    ENUM_TO_STR(VK_ERROR_DEVICE_LOST);
    ENUM_TO_STR(VK_ERROR_MEMORY_MAP_FAILED);
    ENUM_TO_STR(VK_ERROR_LAYER_NOT_PRESENT);
    ENUM_TO_STR(VK_ERROR_EXTENSION_NOT_PRESENT);
    ENUM_TO_STR(VK_ERROR_FEATURE_NOT_PRESENT);
    ENUM_TO_STR(VK_ERROR_INCOMPATIBLE_DRIVER);
    ENUM_TO_STR(VK_ERROR_TOO_MANY_OBJECTS);
    ENUM_TO_STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    ENUM_TO_STR(VK_ERROR_SURFACE_LOST_KHR);
    ENUM_TO_STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    ENUM_TO_STR(VK_SUBOPTIMAL_KHR);
    ENUM_TO_STR(VK_ERROR_OUT_OF_DATE_KHR);
    ENUM_TO_STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    ENUM_TO_STR(VK_ERROR_VALIDATION_FAILED_EXT);
    ENUM_TO_STR(VK_ERROR_INVALID_SHADER_NV);
  default: return "UNKNOWN RESULT";
  }
}

const char *
xrg_log_type_str(xrg_log_type t)
{
  switch (t) {
  case LOG_DEBUG: return "d";
  case LOG_INFO: return "i";
  case LOG_WARNING: return "w";
  case LOG_ERROR: return "e";
  case LOG_FATAL: return "fatal";
  default: return "?";
  }
}

int
xrg_log_type_color(xrg_log_type t)
{
  switch (t) {
  case LOG_DEBUG: return 36;
  case LOG_INFO: return 32;
  case LOG_WARNING: return 33;
  case LOG_ERROR:
  case LOG_FATAL: return 31;
  default: return 36;
  }
}

FILE *
xrg_log_type_stream(xrg_log_type t)
{
#ifdef LOG_TO_STD_ERR
  (void)t;
  return stderr;
#else
  switch (t) {
  case LOG_DEBUG:
  case LOG_INFO:
  case LOG_WARNING: return stdout;
  case LOG_ERROR:
  case LOG_FATAL: return stderr;
  }
#endif
}

#ifdef XR_OS_ANDROID
android_LogPriority
android_level(xrg_log_type t)
{
  switch (t) {
  case LOG_DEBUG: return ANDROID_LOG_DEBUG;
  case LOG_WARNING: return ANDROID_LOG_WARN;
  case LOG_INFO: return ANDROID_LOG_INFO;
  case LOG_ERROR: return ANDROID_LOG_ERROR;
  case LOG_FATAL: return ANDROID_LOG_FATAL;
  default: return ANDROID_LOG_INFO;
  }
}
#endif

void
xrg_log_values(
  const char *file, int line, xrg_log_type t, const char *format, va_list args)
{
#ifdef XR_OS_ANDROID
  __android_log_vprint(android_level(t), "xrgears", format, args);
#else
  FILE *stream = xrg_log_type_stream(t);
#if USE_COLOR
  char code_str[7];
  snprintf(code_str, sizeof(code_str), "\e[%dm", xrg_log_type_color(t));
  fprintf(stream, "%s[%s]%s ", code_str, xrg_log_type_str(t), RESET_COLOR);
#else
  fprintf(stream, "[%s] ", xrg_log_type_str(t));
#endif
  char *fn = strdup(file);
  fprintf(stream, "%s:%d | ", basename(fn), line);
  free(fn);
  vfprintf(stream, format, args);
  fprintf(stream, "\n");
  if (t == LOG_FATAL)
    exit(1);
#endif
}

void
xrg_log_full(
  const char *file, int line, xrg_log_type t, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  xrg_log_values(file, line, t, format, args);
  va_end(args);
}

void
xrg_log_full_if(const char *file,
                int line,
                xrg_log_type t,
                bool cond,
                const char *format,
                ...)
{
  if (!cond)
    return;
  va_list args;
  va_start(args, format);
  xrg_log_values(file, line, t, format, args);
  va_end(args);
}
