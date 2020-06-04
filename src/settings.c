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
}

static const char *
_help_string()
{
  return "A Vulkan OpenXR demo\n"
         "\n"
         "Options:\n"
         "  -g, --gpu GPU            GPU to use (default: 0)\n"
         "  -h, --help               Show this help\n";
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
  int option_index = -1;
  static const char *optstring = "hs:w:vfg:d:m:";


  struct option long_options[] = { { "help", 0, 0, 0 },
                                   { "gpu", 1, 0, 0 },
                                   { 0, 0, 0, 0 } };

  const char *optname;

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, long_options,
                            &option_index)) != -1) {
    if (opt == '?' || opt == ':')
      return false;

    if (option_index != -1)
      optname = long_options[option_index].name;

    if (opt == 'h' || strcmp(optname, "help") == 0) {
      printf("%s\n", _help_string());
      exit(0);
    } else if (opt == 'g' || strcmp(optname, "gpu") == 0) {
      self->gpu = _parse_id(optarg);
    } else {
      xrg_log_f("Unknown option %s", optname);
    }
  }

  if (optind != argc)
    xrg_log_w("trailing args");

  return true;
}
