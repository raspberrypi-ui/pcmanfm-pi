project('pcmanfm', 'c',
  version : '1.4.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.61.0',
  default_options : ['c_std=gnu11'],
)

cc = meson.get_compiler('c')

i18n = import('i18n')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

conf.set10('HAVE_DECL_ENVIRON', cc.has_header_symbol('unistd.h', 'environ'))
conf.set('_LARGEFILE64_SOURCE', 1, description : 'Whether to enable large file support')
conf.set('_FILE_OFFSET_BITS', 64, description : 'File offset bits')

configure_file(output : 'config.h', configuration : conf)
config_h_dir = include_directories('.')
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

localedir = get_option('prefix') / get_option('localedir')
moduledir = get_option('prefix') / get_option('libdir') / 'pcmanfm'
datadir = get_option('prefix') / get_option('datadir') / 'pcmanfm'
uidir = datadir / 'ui'

subdir('src')
subdir('po')
subdir('data')
