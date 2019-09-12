textures = [
  'rooftop_night_4k_tonemapped.png.ktx'
]

glslang = find_program('xxd')

if glslang.found()

  foreach t : textures
    r = run_command('xxd', '-i', t, t + '.h')
    
    if r.returncode() != 0
      message('Could not create texture header:')
      message(r.stderr().strip())
      message(r.stdout().strip())
    endif
  endforeach

else
  message('xxd not found.')
endif
