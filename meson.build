project('xrgears', ['c', 'cpp'], version: '1.0.1',
  meson_version: '>= 0.45.1',
  # designated struct initializers in c++ requires c++2a
  default_options : ['warning_level=2', 'cpp_std=c++2a'],
)

project_args = ['-I' + meson.build_root(),
   '-Wno-missing-field-initializers'
]

compiler = meson.get_compiler('cpp')
compiler_id = compiler.get_id()
if compiler_id == 'clang'
  project_args += [
    '-Wno-c99-extensions'
  ]
endif

add_project_arguments([project_args], language: ['c', 'cpp'])

vulkan_dep = dependency('vulkan')
openxr_dep = dependency('openxr')
glm_dep = dependency('glm')

# needed for clang
m_dep = compiler.find_library('m')
cpp_dep = compiler.find_library('stdc++')

subdir('shaders')
subdir('textures')
subdir('src')

