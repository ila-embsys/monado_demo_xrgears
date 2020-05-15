sources = [
  'gear.hpp',
  'glm_inc.hpp',
  'ktx_stream.c',
  'ktx_stream.h',
  'ktx_texture.c',
  'ktx_texture.h',
  'log.h',
  'log.c',
  'main.cpp',
  'pipeline_equirect.cpp',
  'pipeline_equirect.hpp',
  'pipeline_gears.cpp',
  'pipeline_gears.hpp',
  'settings.hpp',
  'vulkan_buffer.hpp',
  'vulkan_device.hpp',
  'vulkan_framebuffer.hpp',
  'vulkan_instance.hpp',
  'vulkan_shader.hpp',
  'vulkan_texture.hpp',
  'xr.c',
  'xr.h',
  'xr_quad.h',
  'xr_quad.c'
]

include_dirs = [
  include_directories('../shaders'),
  include_directories('../textures')
]

deps = [
  vulkan_dep,
  openxr_dep,
  m_dep,
  cpp_dep
]

executable('xrgears', sources, dependencies: deps,
           include_directories: include_dirs, install: true)
