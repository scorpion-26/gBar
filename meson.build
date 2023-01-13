project('gBar',
    ['cpp'],
    version: '0.0.1',
    license: 'MIT',
    meson_version: '>=0.45.1',
    default_options: ['c_std=c++17', 'warning_level=3'])

gtk = dependency('gtk+-3.0')
gtk_layer_shell = dependency('gtk-layer-shell-0')

if get_option('HasHyprland')
  add_global_arguments('-DHAS_HYPRLAND', language: 'cpp')
endif
if get_option('HasNvidia')
  add_global_arguments('-DHAS_NVIDIA', language: 'cpp')
endif
if get_option('HasBlueZ')
  add_global_arguments('-DHAS_BLUEZ', language: 'cpp')
endif
if get_option('HasSys')
  add_global_arguments('-DHAS_SYS', language: 'cpp')
endif

pulse = dependency('libpulse')

executable(
  'gBar',
  ['src/gBar.cpp', 'src/Window.cpp', 'src/Widget.cpp', 'src/System.cpp', 'src/Bar.cpp', 'src/AudioFlyin.cpp'],
  dependencies: [gtk, gtk_layer_shell, pulse],
  install: true
)
