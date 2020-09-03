sources = [
  'ktx_stream.c',
  'ktx_texture.c',
  'log.c',
  'main.cpp',
  'pipeline_equirect.cpp',
  'pipeline_gears.cpp',
  'xr.c',
  'xr_quad.c',
  'xr_equirect.c',
  'textures.c',
  'vulkan_texture.c',
  'vulkan_buffer.c',
  'vulkan_device.c',
  'vulkan_shader.c',
  'vulkan_context.c',
  'settings.c',
  'vulkan_framebuffer.c',
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
