# Vendored copy of libfm, built directly into the pcmanfm executable (no
# separate libfm/libfm-gtk3 shared libraries, no subproject boundary, no
# external dependency on libfm(-gtk3)(-dev)). See src/meson.build for how
# the resulting static libraries feed into the single `pcmanfm` binary.
#
# `cc`, `i18n`, and the glib/gtk/pango/gio(-unix) dependency objects are
# shared with the parent project's own meson.build (same build, same
# scope) rather than re-declared here.

gnome = import('gnome')

# Absolute install directories, needed to bake paths into the binaries as
# -D defines (datadir/libdir/sysconfdir are relative-to-prefix by default).
prefix_abs = get_option('prefix')
datadir_abs = prefix_abs / get_option('datadir')
libdir_abs = prefix_abs / get_option('libdir')
localedir_abs = datadir_abs / 'locale'
libfm_datadir_abs = datadir_abs / 'libfm'
libfm_uidir_abs = libfm_datadir_abs / 'ui'
libfm_themedir_abs = libfm_datadir_abs / 'images'
libfm_moduledir_abs = libdir_abs / 'libfm' / 'modules'

fm_gobject_dep = dependency('gobject-2.0')
fm_gio2_dep = dependency('gio-2.0', version : '>=2.26.0')
fm_cairo_dep = dependency('cairo', version : '>=1.8.0')
fm_menu_cache_dep = dependency('libmenu-cache', version : '>=0.4.0')
fm_dl_dep = cc.find_library('dl', required : false)

# Combined "GIO_CFLAGS/GIO_LIBS" bundle, matching configure.ac's PKG_CHECK_MODULES(GIO, ...).
fm_gio_deps = [glib_dep, fm_gobject_dep, gthread_dep, fm_gio2_dep, gio_dep]

# Data-dir/locale paths baked into libfm's own sources; kept target-scoped
# (see src/meson.build) rather than project-wide, since they differ from
# pcmanfm's own PACKAGE_DATA_DIR etc.
fm_path_c_args = [
  '-DPACKAGE_LOCALE_DIR="' + localedir_abs + '"',
  '-DPACKAGE_DATA_DIR="' + libfm_datadir_abs + '"',
  '-DPACKAGE_MODULES_DIR="' + libfm_moduledir_abs + '"',
  '-DPACKAGE_UI_DIR="' + libfm_uidir_abs + '"',
  '-DPACKAGE_THEME_DIR="' + libfm_themedir_abs + '"',
]

# libfm's own PACKAGE_VERSION differs from pcmanfm's, so it needs its own
# config.h, resolvable only when compiling libfm's own sources (see
# fm_config_inc below and its use in src/meson.build). GETTEXT_PACKAGE is
# "pcmanfm", not "libfm": libfm's translatable strings are merged into
# po/*.po and served from the single "pcmanfm" gettext domain, so there is
# no separate libfm.mo/libfm/po build any more.
fm_conf = configuration_data()
fm_conf.set_quoted('PACKAGE_VERSION', '1.4.0')
fm_conf.set_quoted('GETTEXT_PACKAGE', gettext_package)
fm_conf.set10('HAVE_DECL_ENVIRON', cc.has_header_symbol('unistd.h', 'environ'))
fm_conf.set('_LARGEFILE64_SOURCE', 1)
fm_conf.set('_FILE_OFFSET_BITS', 64)

configure_file(output : 'config.h', configuration : fm_conf)
fm_config_inc = include_directories('.')

subdir('src')
subdir('data')
