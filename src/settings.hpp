/*
 * xrgears
 *
 * Copyright 2017-2019 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <algorithm>
#include <utility>

#include "log.h"

class Settings
{
public:
  int gpu = -1;

  std::string
  help_string()
  {
    return "A Vulkan OpenXR demo\n"
           "\n"
           "Options:\n"
           "  -g, --gpu GPU            GPU to use (default: 0)\n"
           "  -h, --help               Show this help\n";
  }

  bool
  parse_args(int argc, char *argv[])
  {
    int option_index = -1;
    static const char *optstring = "hs:w:vfg:d:m:";


    struct option long_options[] = { { "help", 0, 0, 0 },
                                     { "gpu", 1, 0, 0 },
                                     { 0, 0, 0, 0 } };

    std::string optname;

    int opt;
    while ((opt = getopt_long(argc, argv, optstring, long_options,
                              &option_index)) != -1) {
      if (opt == '?' || opt == ':')
        return false;

      if (option_index != -1)
        optname = long_options[option_index].name;

      if (opt == 'h' || optname == "help") {
        printf("%s\n", help_string().c_str());
        exit(0);
      } else if (opt == 'g' || optname == "gpu") {
        gpu = parse_id(optarg);
      } else {
        xrg_log_f("Unknown option %s", optname.c_str());
      }
    }

    if (optind != argc)
      xrg_log_w("trailing args");

    return true;
  }

  bool
  is_number(const std::string &str)
  {
    auto is_not_digit = [](char c) { return !std::isdigit(c); };
    return !str.empty() &&
           std::find_if(str.begin(), str.end(), is_not_digit) == str.end();
  }

  int
  parse_id(std::string const &str)
  {
    if (!is_number(str)) {
      xrg_log_e("%s is not a valid number", str.c_str());
      return 0;
    }
    return std::stoi(str, nullptr);
  }
};
