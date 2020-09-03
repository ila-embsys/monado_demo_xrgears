textures = [
  'hawk.ktx',
  'cat.ktx',
  'dresden_station_night_4k.ktx'
]

xxd = find_program('xxd')

if xxd.found()

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
