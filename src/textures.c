#include "textures.h"

#include "log.h"

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
