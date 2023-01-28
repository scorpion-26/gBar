project('gBar',
    ['cpp'],
    version: '0.0.1',
    license: 'MIT',
    meson_version: '>=0.46.0',
    default_options: ['c_std=c++17', 'warning_level=3'])

gtk = dependency('gtk+-3.0')
gtk_layer_shell = dependency('gtk-layer-shell-0')

headers = [
  'src/Common.h',
  'src/System.h',
  'src/PulseAudio.h',
  'src/Widget.h',
  'src/Window.h',
]

if get_option('HasHyprland')
  add_global_arguments('-DHAS_HYPRLAND', language: 'cpp')
  headers += 'src/NvidiaGPU.h'
endif
if get_option('HasNvidia')
  add_global_arguments('-DHAS_NVIDIA', language: 'cpp')
  headers += 'src/Hyprland.h'
endif
if get_option('HasBlueZ')
  add_global_arguments('-DHAS_BLUEZ', language: 'cpp')
endif
if get_option('HasSys')
  add_global_arguments('-DHAS_SYS', language: 'cpp')
endif

pulse = dependency('libpulse')

libgBar = library('gBar', 
  ['src/Window.cpp', 
   'src/Widget.cpp', 
   'src/System.cpp', 
   'src/Bar.cpp', 
   'src/AudioFlyin.cpp',
   'src/BluetoothDevices.cpp',
   'src/Plugin.cpp'
  ],
  dependencies: [gtk, gtk_layer_shell, pulse],
  install: true)

pkg = import('pkgconfig')
pkg.generate(libgBar)

executable(
  'gBar',
  ['src/gBar.cpp'], 
  dependencies: [gtk],
  link_with: libgBar,
  install: true
)

install_headers(
  headers,
  subdir: 'gBar'
)

install_data(
  ['css/style.css',
   'css/style.scss'],
  install_dir: get_option('datadir') / 'gBar'
)
