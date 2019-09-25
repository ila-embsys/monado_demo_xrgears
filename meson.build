project('xrgears', ['c', 'cpp'], version: '1.0.1',
  meson_version: '>= 0.45.1'
)

project_args = ['-I' + meson.build_root(),
   '-Wall',
   '-Wextra',
   '-Wno-missing-field-initializers'
]

compiler = meson.get_compiler('cpp')
compiler_id = compiler.get_id()

if compiler_id == 'clang'
  project_args += [
    '-Weverything',
    '-Wno-shorten-64-to-32',
    '-Wno-double-promotion',
    '-Wno-implicit-float-conversion',
    '-Wno-sign-conversion',
    '-Wno-c99-extensions',
    '-Wno-c++98-compat',
    '-Wno-c++98-compat-pedantic',
    '-Wno-old-style-cast',
    '-Wno-shadow',
    '-Wno-padded',
    '-Wno-extra-semi-stmt',
    '-Wno-zero-as-null-pointer-constant',
    '-Wno-switch-enum',
    '-Wno-covered-switch-default',
    '-Wno-weak-vtables',
    '-Wno-vla',
    '-Wno-pedantic',
    '-Wno-missing-prototypes',
    '-Wno-format-nonliteral',
    # ktx
    '-Wno-documentation-unknown-command',
    '-Wno-reserved-id-macro',
    '-Wno-documentation',
    # generated asset headers,
    '-Wno-newline-eof',
    '-Wno-missing-variable-declarations'
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

