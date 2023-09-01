project('gBarHelloWorld',
        ['cpp'],
        version: '0.0.1',
        license: 'MIT',
        meson_version: '>=0.45.1',
        default_options: ['c_std=c++17', 'warning_level=3'])

gBar = dependency('gBar')

add_global_arguments('-DUSE_LOGFILE', language: 'cpp')

library(
  'gBarHelloWorld',
  ['main.cpp'],
  dependencies: [gBar],
  install: true,
  install_dir: 'lib/gBar/')
