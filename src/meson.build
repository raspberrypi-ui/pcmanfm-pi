# Vendored copy of libfm (src/libfm/) plus pcmanfm's own sources
# (src/pcmanfm/), compiled directly into a single `pcmanfm` executable in
# one go: no intermediate libfm/libfm-gtk3 static or shared libraries, no
# subproject boundary, no external dependency on libfm(-gtk3)(-dev).
#
# `cc`, `i18n`, and the glib/gtk/pango/gio(-unix) dependency objects are
# shared with the parent project's own meson.build (same build, same
# scope) rather than re-declared here.

gnome = import('gnome')

# Shared with pcmanfm's own module directory (see root meson.build): libfm
# is no longer an installable/shared library other applications could load
# modules from, so there is no more reason for it to have a separate
# default scan directory from pcmanfm's own.
libfm_moduledir_abs = pcmanfm_moduledir_abs
# PACKAGE_DATA_DIR/PACKAGE_UI_DIR are pcmanfm's own (see root meson.build);
# libfm's data/ui files install alongside pcmanfm's, not to a separate
# datadir/libfm.
libfm_themedir_abs = pcmanfm_datadir_abs / 'images'

fm_gobject_dep = dependency('gobject-2.0')
fm_gio2_dep = dependency('gio-2.0', version : '>=2.26.0')
fm_cairo_dep = dependency('cairo', version : '>=1.8.0')
fm_menu_cache_dep = dependency('libmenu-cache', version : '>=0.4.0')
fm_dl_dep = cc.find_library('dl', required : false)

# Locale/module/theme paths baked into libfm's own sources. PACKAGE_DATA_DIR/
# PACKAGE_UI_DIR/PACKAGE_LOCALE_DIR are pcmanfm's own shared values (see root
# meson.build) - libfm has no separate datadir/libfm any more.
fm_path_c_args = [
  '-DPACKAGE_LOCALE_DIR="' + pcmanfm_localedir_abs + '"',
  '-DPACKAGE_DATA_DIR="' + pcmanfm_datadir_abs + '"',
  '-DPACKAGE_MODULES_DIR="' + libfm_moduledir_abs + '"',
  '-DPACKAGE_UI_DIR="' + pcmanfm_uidir_abs + '"',
  '-DPACKAGE_THEME_DIR="' + libfm_themedir_abs + '"',
]

# fm-version.h is a generated header (see below), found via fm_config_inc;
# config.h itself is the single, project-wide one from root meson.build
# (config_h_dir) - there is only one executable target now, so there can
# only be one config.h shared by every source file.
inc_top = include_directories('libfm')
inc_common = include_directories('libfm/base', 'libfm/job')
inc_gtk = include_directories('libfm/gtk')
inc_extra = include_directories('libfm/extra')

# PACKAGE_*_DIR/cast-checks/warnings for every source file, libfm's and
# pcmanfm's alike, since they are all compiled into the single executable
# target below and a target's c_args apply uniformly to all of its sources.
pcmanfm_c_args = fm_path_c_args + ['-Wall'] + g_cast_checks_args
if cc.has_argument('-Werror-implicit-function-declaration')
  pcmanfm_c_args += ['-Werror-implicit-function-declaration']
endif

base_sources = files(
  'libfm/base/fm-action.c',
  'libfm/base/fm-app-info.c',
  'libfm/base/fm-archiver.c',
  'libfm/base/fm-bookmarks.c',
  'libfm/base/fm-config.c',
  'libfm/base/fm-dummy-monitor.c',
  'libfm/base/fm-file.c',
  'libfm/base/fm-file-info.c',
  'libfm/base/fm-file-launcher.c',
  'libfm/base/fm-folder.c',
  'libfm/base/fm-folder-config.c',
  'libfm/base/fm-icon.c',
  'libfm/base/fm-list.c',
  'libfm/base/fm-mime-type.c',
  'libfm/base/fm-module.c',
  'libfm/base/fm-monitor.c',
  'libfm/base/fm-nav-history.c',
  'libfm/base/fm-path.c',
  'libfm/base/fm-templates.c',
  'libfm/base/fm-terminal.c',
  'libfm/base/fm-thumbnail-loader.c',
  'libfm/base/fm-thumbnailer.c',
  'libfm/base/fm-utils.c',
)

job_sources = files(
  'libfm/job/fm-deep-count-job.c',
  'libfm/job/fm-dir-list-job.c',
  'libfm/job/fm-file-info-job.c',
  'libfm/job/fm-file-ops-job.c',
  'libfm/job/fm-file-ops-job-change-attr.c',
  'libfm/job/fm-file-ops-job-delete.c',
  'libfm/job/fm-file-ops-job-xfer.c',
  'libfm/job/fm-job.c',
)

# libfm-extra's sole source (used directly by the vfs-menu module); folded
# in alongside base/job since there is no longer a separate installable
# libfm-extra.
extra_sources = files('libfm/extra/fm-xml-file.c')

fm_marshal = gnome.genmarshal('fm-marshal',
  sources : 'libfm/base/fm-marshal.list',
  prefix : 'fm_marshal',
)

configure_file(
  input : 'libfm/fm-version.h.in',
  output : 'fm-version.h',
  configuration : {
    'FM_VERSION_MAJOR': 1,
    'FM_VERSION_MINOR': 4,
    'FM_VERSION_MICRO': 0,
  },
)
fm_config_inc = include_directories('.')

libfm_sources = files('libfm/fm.c') + base_sources + job_sources + extra_sources + fm_marshal

fm_gtk_marshal = gnome.genmarshal('fm-gtk-marshal',
  sources : 'libfm/gtk/fm-gtk-marshal.list',
  prefix : 'fm_marshal',
)

exo_sources = files(
  'libfm/gtk/exo/exo-icon-view.c',
  'libfm/gtk/exo/exo-private.c',
  'libfm/gtk/exo/exo-tree-view.c',
  'libfm/gtk/exo/exo-icon-view.h',
  'libfm/gtk/exo/exo-private.h',
  'libfm/gtk/exo/exo-tree-view.h',
  'libfm/gtk/exo/exo-marshal.c',
  'libfm/gtk/exo/exo-marshal.h',
)

# libfm's own bundled modules (previously separate dlopen()ed "*.so" files
# under a modules directory). Compiled directly in here instead, and wired
# up with plain direct calls at their call sites (see the _init()
# functions in base/fm-file.c, gtk/fm-folder-view.c and
# gtk/fm-file-properties.c) so the executable does not need to dlopen
# anything nor export any symbols for them to work.
builtin_module_sources = files(
  'libfm/modules/vfs-menu.c',
  'libfm/modules/vfs-search.c',
  'libfm/modules/gtk-fileprop-x-desktop.c',
  'libfm/modules/gtk-fileprop-x-shortcut.c',
  'libfm/modules/gtk-menu-trash.c',
  'libfm/modules/gtk-menu-actions.c',
)

libfm_gtk_sources = files('libfm/fm-gtk.c') + exo_sources + files(
  'libfm/gtk/fm-app-chooser-combo-box.c',
  'libfm/gtk/fm-app-chooser-dlg.c',
  'libfm/gtk/fm-app-menu-view.c',
  'libfm/gtk/fm-cell-renderer-pixbuf.c',
  'libfm/gtk/fm-cell-renderer-text.c',
  'libfm/gtk/fm-clipboard.c',
  'libfm/gtk/fm-dir-tree-model.c',
  'libfm/gtk/fm-dir-tree-view.c',
  'libfm/gtk/fm-dnd-auto-scroll.c',
  'libfm/gtk/fm-dnd-dest.c',
  'libfm/gtk/fm-dnd-src.c',
  'libfm/gtk/fm-file-menu.c',
  'libfm/gtk/fm-file-properties.c',
  'libfm/gtk/fm-folder-model.c',
  'libfm/gtk/fm-folder-view.c',
  'libfm/gtk/fm-gtk-file-launcher.c',
  'libfm/gtk/fm-gtk-utils.c',
  'libfm/gtk/fm-icon-pixbuf.c',
  'libfm/gtk/fm-menu-tool-item.c',
  'libfm/gtk/fm-path-bar.c',
  'libfm/gtk/fm-path-entry.c',
  'libfm/gtk/fm-places-model.c',
  'libfm/gtk/fm-places-view.c',
  'libfm/gtk/fm-progress-dlg.c',
  'libfm/gtk/fm-side-pane.c',
  'libfm/gtk/fm-standard-view.c',
  'libfm/gtk/fm-tab-label.c',
  'libfm/gtk/fm-thumbnail.c',
) + fm_gtk_marshal + builtin_module_sources

pcmanfm_sources = files(
  'pcmanfm/pcmanfm.c',
  'pcmanfm/app-config.c',
  'pcmanfm/main-win.c',
  'pcmanfm/tab-page.c',
  'pcmanfm/desktop.c',
  'pcmanfm/volume-manager.c',
  'pcmanfm/pref.c',
  'pcmanfm/single-inst.c',
  'pcmanfm/connect-server.c',
)

# Full link-dependency list for the pcmanfm binary: its own direct deps
# plus the ones libfm's sources need, all compiled into one target so
# there is one combined dependency list rather than one list per
# static_library() as before.
pcmanfm_deps = [
  gio_dep, gthread_dep, glib_dep, pango_dep, layer_shell_dep, gtk_dep, m_dep,
  fm_gobject_dep, fm_gio2_dep, fm_cairo_dep, fm_menu_cache_dep, fm_dl_dep,
]

executable('pcmanfm',
  pcmanfm_sources + libfm_sources + libfm_gtk_sources,
  c_args : pcmanfm_c_args,
  include_directories : [config_h_dir, fm_config_inc, inc_top, inc_common, inc_gtk, inc_extra],
  dependencies : pcmanfm_deps + [x11_dep],
  install : true,
)

install_headers('pcmanfm/pcmanfm-modules.h')

# prepare empty modules directory, matching the autotools install-exec-local
install_emptydir(pcmanfm_modules_dir)
