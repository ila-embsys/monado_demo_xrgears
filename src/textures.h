/*
 * xrgears
 *
 * Copyright 2020 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <gio/gio.h>
const char*
gio_get_asset(const gchar* path, gsize* size);

#ifdef __cplusplus
}
#endif
