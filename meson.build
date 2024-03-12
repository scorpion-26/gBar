project('gBar',
    ['c', 'cpp'],
    version: '0.0.1',
    license: 'MIT',
    meson_version: '>=0.53.0',
    default_options: ['cpp_std=c++17',
                      'warning_level=3',
                      'default_library=static',
                      'buildtype=release'],
)

# Wayland protocols
wayland_client = dependency('wayland-client')
wayland_scanner = find_program('wayland-scanner')

ext_workspace_src = custom_target('generate-ext-workspace-src',
                                  input: ['protocols/ext-workspace-unstable-v1.xml'],
                                  output: ['ext-workspace-unstable-v1.c'],
                                  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])

ext_workspace_header = custom_target('generate-ext-workspace-header',
                                  input: ['protocols/ext-workspace-unstable-v1.xml'],
                                  output: ['ext-workspace-unstable-v1.h'],
                                  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])

gtk = dependency('gtk+-3.0')
gtk_layer_shell = dependency('gtk-layer-shell-0')

pulse = dependency('libpulse')


comp = meson.get_compiler('cpp')

sass_lib = comp.find_library('libsass')


headers = [
  'src/Common.h',
  'src/Log.h',
  'src/System.h',
  'src/PulseAudio.h',
  'src/Widget.h',
  'src/Window.h',
  'src/Wayland.h',
  'src/Config.h',
  'src/CSS.h'
]

sources = [
    ext_workspace_src,
    ext_workspace_header,
   'src/Window.cpp',
   'src/Widget.cpp',
   'src/Wayland.cpp',
   'src/System.cpp',
   'src/Bar.cpp',
   'src/Workspaces.cpp',
   'src/AudioFlyin.cpp',
   'src/BluetoothDevices.cpp',
   'src/Plugin.cpp',
   'src/Config.cpp',
   'src/CSS.cpp',
   'src/Log.cpp',
   'src/SNI.cpp',
   ]

dependencies = [gtk, gtk_layer_shell, pulse, wayland_client, sass_lib]

if get_option('WithHyprland')
  add_global_arguments('-DWITH_HYPRLAND', language: 'cpp')
  headers += 'src/Workspaces.h'
endif
if get_option('WithWorkspaces')
  add_global_arguments('-DWITH_WORKSPACES', language: 'cpp')
  headers += 'src/Workspaces.h'
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
if get_option('WithSNI')
  add_global_arguments('-DWITH_SNI', language: 'cpp')

  # DBus protocols
  dbus_gen = find_program('gdbus-codegen')
  sni_item_src = custom_target('generate-sni-item-src',
                               input: ['protocols/sni-item.xml'],
                               output: ['sni-item.c'],
                               command: [dbus_gen, '--c-namespace', 'sni', '--body', '--output', '@OUTPUT@', '@INPUT@'])

  sni_item_header = custom_target('generate-sni-item-header',
                               input: ['protocols/sni-item.xml'],
                               output: ['sni-item.h'],
                               command: [dbus_gen, '--c-namespace', 'sni', '--header', '--output', '@OUTPUT@', '@INPUT@'])

  sni_watcher_src = custom_target('generate-sni-watcher-src',
                               input: ['protocols/sni-watcher.xml'],
                               output: ['sni-watcher.c'],
                               command: [dbus_gen, '--c-namespace', 'sni', '--body', '--output', '@OUTPUT@', '@INPUT@'])

  sni_watcher_header = custom_target('generate-sni-watcher-header',
                               input: ['protocols/sni-watcher.xml'],
                               output: ['sni-watcher.h'],
                               command: [dbus_gen, '--c-namespace', 'sni', '--header', '--output', '@OUTPUT@', '@INPUT@'])
  libdbusmenu = dependency('dbusmenu-gtk3-0.4')

  sources += sni_item_src
  sources += sni_item_header
  sources += sni_watcher_src
  sources += sni_watcher_header
  
  dependencies += libdbusmenu
endif

add_global_arguments('-DUSE_LOGFILE', language: 'cpp')

# stb
fs = import('fs')
stb = include_directories('thirdparty')
add_global_arguments('-DHAS_STB', language: 'cpp')


libgBar = library('gBar',
  sources,
  dependencies: dependencies,
  include_directories: stb,
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
