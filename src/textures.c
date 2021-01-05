#include "textures.h"

#include "log.h"

#ifdef XR_OS_ANDROID
#include <android/asset_manager_jni.h>

bool
android_context_init(android_context *context,
                     JavaVM *vm,
                     JNIEnv *env,
                     jobject activity)
{
  context->vm = vm;
  context->env = env;
  context->activity = activity;

  (*vm)->AttachCurrentThread(vm, &env, NULL);

  jclass activity_class = (*env)->GetObjectClass(env, context->activity);

  jmethodID activity_class_getAssets = (*env)->GetMethodID(
    env, activity_class, "getAssets", "()Landroid/content/res/AssetManager;");
  jobject asset_manager =
    (*env)->CallObjectMethod(env, context->activity, activity_class_getAssets);
  jobject global_asset_manager = (*env)->NewGlobalRef(env, asset_manager);

  context->mgr = AAssetManager_fromJava(env, global_asset_manager);

  return true;
}

char *
android_get_asset(android_context *context,
                  const char *file_name,
                  size_t *length)
{
  AAsset *asset =
    AAssetManager_open(context->mgr, file_name, AASSET_MODE_BUFFER);
  if (asset) {
    *length = AAsset_getLength(asset);

    xrg_log_d("Asset '%s' file size: %zu", file_name, *length);

    char *buffer = (char *)malloc(*length + 1);
    AAsset_read(asset, buffer, *length);
    buffer[*length] = 0;
    AAsset_close(asset);
    // free(buffer);
    return buffer;
  } else {
    xrg_log_e("Cannot open asset file '%s'", file_name);
    return NULL;
  }
}
#endif

#ifndef XR_OS_ANDROID

#include <gio/gio.h>

static gboolean
_load_resource(const gchar *path, GBytes **res)
{
  GError *error = NULL;
  *res = g_resources_lookup_data(path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (error != NULL) {
    g_printerr("Unable to read file: %s\n", error->message);
    g_error_free(error);
    return FALSE;
  }

  return TRUE;
}

const char *
gio_get_asset(const gchar *path, gsize *size)
{

  GBytes *bytes = NULL;

  if (!_load_resource(path, &bytes)) {
    xrg_log_e("Could not load resource %s", path);
  }

  const gchar *data = g_bytes_get_data(bytes, size);

  return data;
}
#endif
