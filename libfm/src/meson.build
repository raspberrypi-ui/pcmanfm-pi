inc_top = include_directories('.')
inc_common = include_directories('base', 'job')
inc_gtk = include_directories('gtk')

# GETTEXT_PACKAGE/PACKAGE_VERSION/PACKAGE_*_DIR/cast-checks for libfm's own
# sources; scoped to these two targets only (not project-wide) since they
# differ from pcmanfm's own equivalents.
fm_extra_c_args = fm_path_c_args + g_cast_checks_args
if cc.has_argument('-Werror-implicit-function-declaration')
  fm_extra_c_args += ['-Werror-implicit-function-declaration']
endif

base_sources = files(
  'base/fm-action.c',
  'base/fm-app-info.c',
  'base/fm-archiver.c',
  'base/fm-bookmarks.c',
  'base/fm-config.c',
  'base/fm-dummy-monitor.c',
  'base/fm-file.c',
  'base/fm-file-info.c',
  'base/fm-file-launcher.c',
  'base/fm-folder.c',
  'base/fm-folder-config.c',
  'base/fm-icon.c',
  'base/fm-list.c',
  'base/fm-mime-type.c',
  'base/fm-module.c',
  'base/fm-monitor.c',
  'base/fm-nav-history.c',
  'base/fm-path.c',
  'base/fm-templates.c',
  'base/fm-terminal.c',
  'base/fm-thumbnail-loader.c',
  'base/fm-thumbnailer.c',
  'base/fm-utils.c',
)

job_sources = files(
  'job/fm-deep-count-job.c',
  'job/fm-dir-list-job.c',
  'job/fm-file-info-job.c',
  'job/fm-file-ops-job.c',
  'job/fm-file-ops-job-change-attr.c',
  'job/fm-file-ops-job-delete.c',
  'job/fm-file-ops-job-xfer.c',
  'job/fm-job.c',
)

# libfm-extra's sole source (used directly by the vfs-menu module); folded
# into the same static library as base/job since there is no longer a
# separate installable libfm-extra.
extra_sources = files('extra/fm-xml-file.c')

fm_marshal = gnome.genmarshal('fm-marshal',
  sources : 'base/fm-marshal.list',
  prefix : 'fm_marshal',
)

configure_file(
  input : 'fm-version.h.in',
  output : 'fm-version.h',
  configuration : {
    'FM_VERSION_MAJOR': 1,
    'FM_VERSION_MINOR': 4,
    'FM_VERSION_MICRO': 0,
  },
)

libfm_sources = files('fm.c') + base_sources + job_sources + extra_sources + fm_marshal

libfm_lib = static_library('fm-core', libfm_sources,
  include_directories : [fm_config_inc, inc_common],
  dependencies : fm_gio_deps + [fm_menu_cache_dep, m_dep, fm_dl_dep],
  c_args : fm_extra_c_args,
)

fm_gtk_marshal = gnome.genmarshal('fm-gtk-marshal',
  sources : 'gtk/fm-gtk-marshal.list',
  prefix : 'fm_marshal',
)

exo_sources = files(
  'gtk/exo/exo-icon-view.c',
  'gtk/exo/exo-private.c',
  'gtk/exo/exo-tree-view.c',
  'gtk/exo/exo-icon-view.h',
  'gtk/exo/exo-private.h',
  'gtk/exo/exo-tree-view.h',
  'gtk/exo/exo-marshal.c',
  'gtk/exo/exo-marshal.h',
  'gtk/exo/exo-string.h',
)

inc_extra = include_directories('extra')

# libfm's own bundled modules (previously separate dlopen()ed "*.so" files
# under a modules directory). Compiled directly in here instead, and
# statically registered via fm_module_register_builtin() (see the
# _init() functions in base/fm-file.c, gtk/fm-folder-view.c and
# gtk/fm-file-properties.c) so the executable does not need to dlopen
# anything nor export any symbols for them to work.
builtin_module_sources = files(
  'modules/vfs-menu.c',
  'modules/vfs-search.c',
  'modules/gtk-fileprop-x-desktop.c',
  'modules/gtk-fileprop-x-shortcut.c',
  'modules/gtk-menu-trash.c',
  'modules/gtk-menu-actions.c',
)

libfm_gtk_sources = files('fm-gtk.c') + exo_sources + files(
  'gtk/fm-app-chooser-combo-box.c',
  'gtk/fm-app-chooser-dlg.c',
  'gtk/fm-app-menu-view.c',
  'gtk/fm-cell-renderer-pixbuf.c',
  'gtk/fm-cell-renderer-text.c',
  'gtk/fm-clipboard.c',
  'gtk/fm-dir-tree-model.c',
  'gtk/fm-dir-tree-view.c',
  'gtk/fm-dnd-auto-scroll.c',
  'gtk/fm-dnd-dest.c',
  'gtk/fm-dnd-src.c',
  'gtk/fm-file-menu.c',
  'gtk/fm-file-properties.c',
  'gtk/fm-folder-model.c',
  'gtk/fm-folder-view.c',
  'gtk/fm-gtk-file-launcher.c',
  'gtk/fm-gtk-utils.c',
  'gtk/fm-icon-pixbuf.c',
  'gtk/fm-menu-tool-item.c',
  'gtk/fm-path-bar.c',
  'gtk/fm-path-entry.c',
  'gtk/fm-places-model.c',
  'gtk/fm-places-view.c',
  'gtk/fm-progress-dlg.c',
  'gtk/fm-side-pane.c',
  'gtk/fm-standard-view.c',
  'gtk/fm-tab-label.c',
  'gtk/fm-thumbnail.c',
) + fm_gtk_marshal + builtin_module_sources

libfm_gtk_lib = static_library('fm-gtk', libfm_gtk_sources,
  include_directories : [fm_config_inc, inc_top, inc_common, inc_gtk, inc_extra],
  dependencies : [gtk_dep, pango_dep, fm_cairo_dep, fm_menu_cache_dep, m_dep] + fm_gio_deps,
  link_with : libfm_lib,
  c_args : fm_extra_c_args,
)
