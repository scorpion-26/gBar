project('gBar',
    ['cpp'],
    version: '0.0.1',
    license: 'MIT',
    meson_version: '>=0.49.0',
    default_options: ['c_std=c++17',
                      'warning_level=3',
                      'default_library=static',
                      'buildtype=release'],
)

gtk = dependency('gtk+-3.0')
gtk_layer_shell = dependency('gtk-layer-shell-0')

headers = [
  'src/Common.h',
  'src/Log.h',
  'src/System.h',
  'src/PulseAudio.h',
  'src/Widget.h',
  'src/Window.h',
  'src/Config.h',
  'src/CSS.h'
]

if get_option('WithHyprland')
  add_global_arguments('-DWITH_HYPRLAND', language: 'cpp')
  headers += 'src/Hyprland.h'
endif
if get_option('WithNvidia')
  add_global_arguments('-DWITH_NVIDIA', language: 'cpp')
  headers += 'src/NvidiaGPU.h'
endif
if get_option('WithAMD')
  add_global_arguments('-DWITH_AMD', language: 'cpp')
  headers += 'src/AMDGPU.h'
endif
if get_option('WithBlueZ')
  add_global_arguments('-DWITH_BLUEZ', language: 'cpp')
endif
if get_option('WithSys')
  add_global_arguments('-DWITH_SYS', language: 'cpp')
endif

add_global_arguments('-DUSE_LOGFILE', language: 'cpp')

pulse = dependency('libpulse')

libgBar = library('gBar', 
  ['src/Window.cpp', 
   'src/Widget.cpp', 
   'src/System.cpp', 
   'src/Bar.cpp', 
   'src/AudioFlyin.cpp',
   'src/BluetoothDevices.cpp',
   'src/Plugin.cpp',
   'src/Config.cpp',
   'src/CSS.cpp',
   'src/Log.cpp'
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
  install_rpath: '$ORIGIN/',
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
