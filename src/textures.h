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

#ifndef XR_OS_ANDROID
#include <gio/gio.h>
const char*
gio_get_asset(const gchar* path, gsize* size);
#else
#include <jni.h>
#include <android/asset_manager.h>

typedef struct
{
  JavaVM* vm;
  JNIEnv* env;
  jobject activity;
  AAssetManager* mgr;
} android_context;

static android_context global_android_context;
bool
android_context_init(android_context* context,
                     JavaVM* vm,
                     JNIEnv* env,
                     jobject activity);
char*
android_get_asset(android_context* context,
                  const char* file_name,
                  size_t* length);
#endif

#ifdef __cplusplus
}
#endif
