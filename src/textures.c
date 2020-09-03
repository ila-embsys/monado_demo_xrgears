#include "textures.h"

#include "cat.ktx.h"
#include "hawk.ktx.h"
#include "rooftop_night_4k_tonemapped.png.ktx.h"
#include "dresden_station_night_4k.ktx.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

ktx_size_t
rooftop_size()
{
  return ARRAY_SIZE(rooftop_night_4k_tonemapped_png_ktx);
}
const ktx_uint8_t*
rooftop_bytes()
{
  return rooftop_night_4k_tonemapped_png_ktx;
}

ktx_size_t
cat_size()
{
  return ARRAY_SIZE(cat_ktx);
}
const ktx_uint8_t*
cat_bytes()
{
  return cat_ktx;
}

ktx_size_t
hawk_size()
{
  return ARRAY_SIZE(hawk_ktx);
}
const ktx_uint8_t*
hawk_bytes()
{
  return hawk_ktx;
}

ktx_size_t
station_size()
{
  return ARRAY_SIZE(dresden_station_night_4k_ktx);
}
const ktx_uint8_t*
station_bytes()
{
  return dresden_station_night_4k_ktx;
}
