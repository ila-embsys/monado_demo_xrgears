/*
 * xrgears
 *
 * Copyright 2017-2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "settings.h"

#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "log.h"

static void
_init(xrg_settings *self)
{
  self->gpu = -1;
  self->vulkan_enable2 = true;
  self->enable_gears = true;
  self->enable_quad = true;
  self->enable_sky = true;
}

static const char *
_help_string()
{
  return "A Vulkan OpenXR demo\n"
         "\n"
         "Options:\n"
         "  -d GPU     GPU to use (default: 0)\n"
         "  -1         Use XR_KHR_vulkan_enable instead of "
         "XR_KHR_vulkan_enable2\n"
         "  -s         Disable sky layer\n"
         "  -q         Disable quad layers\n"
         "  -g         Disable gears layer\n"
         "  -o         Enable overlay support\n"
         "  -h         Show this help\n";
}

static int
_parse_id(const char *str)
{
  if (isdigit(str[0]) == 0) {
    xrg_log_e("%s is not a valid number", str);
    return 0;
  }
  return atoi(str);
}

bool
settings_parse_args(xrg_settings *self, int argc, char *argv[])
{
  _init(self);
  static const char *optstring = "h1d:sqgo";

  int opt;
  while ((opt = getopt(argc, argv, optstring)) != -1) {
    if (opt == '?' || opt == ':')
      return false;

    if (opt == 'h') {
      printf("%s\n", _help_string());
      exit(0);
    } else if (opt == 'd') {
      self->gpu = _parse_id(optarg);
    } else if (opt == '1') {
      self->vulkan_enable2 = false;
    } else if (opt == 's') {
      self->enable_sky = false;
    } else if (opt == 'q') {
      self->enable_quad = false;
    } else if (opt == 'g') {
      self->enable_gears = false;
    } else if (opt == 'o') {
      self->enable_overlay = true;
    } else {
      xrg_log_f("Unknown option %c", opt);
    }
  }

  if (optind != argc)
    xrg_log_w("trailing args");

  return true;
}
