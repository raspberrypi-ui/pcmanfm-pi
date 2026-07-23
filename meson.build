project('pcmanfm', 'c',
  version : '1.4.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.61.0',
  default_options : ['c_std=gnu11'],
)

i18n = import('i18n')

localedir = get_option('prefix') / get_option('localedir')
moduledir = get_option('prefix') / get_option('libdir') / 'pcmanfm'
datadir = get_option('prefix') / get_option('datadir') / 'pcmanfm'
uidir = datadir / 'ui'
themedir = datadir / 'images'

subdir('src')
subdir('po')
subdir('data')
