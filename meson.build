project('pcmanfm', 'c',
  version : '1.4.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.61.0',
  default_options : ['c_std=gnu11'],
)

cc = meson.get_compiler('c')
i18n = import('i18n')

gettext_package = 'pcmanfm'

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', gettext_package)

# Large file support
conf.set('_LARGEFILE64_SOURCE', 1, description : 'Whether to enable large file support')
conf.set('_FILE_OFFSET_BITS', 64, description : 'File offset bits')

g_cast_checks_args = ['-DG_DISABLE_CAST_CHECKS']

extra_c_args = [
  '-D_LARGEFILE_SOURCE', '-D_REENTRANT', '-D_FILE_OFFSET_BITS=64',
  # turn off glib debug checks
  '-DG_DISABLE_ASSERT', '-DG_DISABLE_CHECKS',
  # gtk-layer-shell only supports GTK+ 3, so warnings about deprecated GTK+ 3
  # API are expected and silenced here, same as the old --with-gtk=3 path.
  '-Wno-deprecated-declarations',
]

add_project_arguments(extra_c_args, language : 'c')

configure_file(output : 'config.h', configuration : conf)
config_h_dir = include_directories('.')
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

x11_dep = dependency('x11')
# version bumped to satisfy src/libfm/meson.build's own requirement too,
# since it reuses this same dependency object (see subdir('src/libfm') below)
gio_dep = dependency('gio-unix-2.0', version : '>= 2.26.0')
gthread_dep = dependency('gthread-2.0')
glib_dep = dependency('glib-2.0')
pango_dep = dependency('pango', version : '>= 1.20.0')
layer_shell_dep = dependency('gtk-layer-shell-0', version : '>= 0.6')
gtk_dep = dependency('gtk+-3.0')
m_dep = cc.find_library('m', required : false)

# Single shared data/ui install location. libfm's own data/ui files install
# here too (see src/libfm/meson.build) rather than a separate datadir/libfm,
# since there is no longer a standalone libfm.
pcmanfm_datadir_abs = get_option('prefix') / get_option('datadir') / 'pcmanfm'
pcmanfm_uidir_abs = pcmanfm_datadir_abs / 'ui'
# Shared with src/libfm/meson.build too, so both honour the localedir option
# the same way rather than libfm re-deriving it from datadir.
pcmanfm_localedir_abs = get_option('prefix') / get_option('localedir')

# Single shared third-party dlopen()-module directory. libfm no longer
# exists as an installable/shared library other applications could load
# modules from, so there is no longer a reason for libfm's own default
# module directory (see src/libfm/meson.build) to differ from pcmanfm's.
pcmanfm_modules_dir = get_option('libdir') / 'pcmanfm'
pcmanfm_moduledir_abs = get_option('prefix') / pcmanfm_modules_dir

# Vendored in src/libfm/ and compiled directly into this same build (no
# subproject boundary, no separate libfm(-gtk3) shared libraries) so this
# project has no external dependency on libfm(-gtk3)(-dev). Provides the
# libfm_lib/libfm_gtk_lib static libraries and inc_top/inc_common/inc_gtk
# include dirs consumed by src/pcmanfm/meson.build below.
subdir('src/libfm')

# Full link-dependency list for the pcmanfm binary itself (src/pcmanfm/
# meson.build): its own direct deps plus the ones libfm's static libraries
# were compiled against, since linking a static library into an executable
# doesn't pull those along automatically - the final link still needs them
# supplied explicitly. Only assemblable after subdir('src/libfm') has run,
# since the fm_* ones are declared there.
pcmanfm_deps = [
  gio_dep, gthread_dep, glib_dep, pango_dep, layer_shell_dep, gtk_dep, m_dep,
  fm_gobject_dep, fm_gio2_dep, fm_cairo_dep, fm_menu_cache_dep, fm_dl_dep,
]

subdir('src/pcmanfm')
subdir('po')
subdir('data')
