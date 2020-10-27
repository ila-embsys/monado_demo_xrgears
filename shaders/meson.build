shaders = [
  'gears.vert',
  'gears.frag',
  'sky_plane_equirect.frag',
  'sky_plane_equirect.vert'
]

glslang = find_program('glslangValidator')

if glslang.found()
  foreach s : shaders
    r = run_command('glslangValidator', '-V', '-o', s + '.h', s, '--vn', s.underscorify())
    if r.returncode() != 0
      message('Could not compile shaders:')
      message(r.stderr().strip())
      message(r.stdout().strip())
    endif
  endforeach
else
  message('glslangValidator not found.')
endif
