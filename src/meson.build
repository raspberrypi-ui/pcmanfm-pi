gnome = import('gnome')

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

extra_sources = files(
  'libfm/extra/fm-xml-file.c'
)

fm_marshal = gnome.genmarshal('fm-marshal',
  sources : 'libfm/base/fm-marshal.list',
  prefix : 'fm_marshal',
)

libfm_sources = files('libfm/fm.c') + base_sources + job_sources + extra_sources + fm_marshal

gtk_sources = files(
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

module_sources = files(
  'libfm/modules/vfs-menu.c',
  'libfm/modules/vfs-search.c',
  'libfm/modules/gtk-fileprop-x-desktop.c',
  'libfm/modules/gtk-fileprop-x-shortcut.c',
  'libfm/modules/gtk-menu-trash.c',
  'libfm/modules/gtk-menu-actions.c',
)

fm_gtk_marshal = gnome.genmarshal('fm-gtk-marshal',
  sources : 'libfm/gtk/fm-gtk-marshal.list',
  prefix : 'fm_marshal',
)

libfm_gtk_sources = files('libfm/fm-gtk.c') + gtk_sources + exo_sources + module_sources + fm_gtk_marshal

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

path_c_args = [
  '-DPACKAGE_LOCALE_DIR="' + localedir + '"',
  '-DPACKAGE_DATA_DIR="' + datadir + '"',
  '-DPACKAGE_MODULES_DIR="' + moduledir + '"',
  '-DPACKAGE_UI_DIR="' + uidir + '"',
  '-DPACKAGE_THEME_DIR="' + datadir + '/images"',
]

extra_c_args = [
  '-D_LARGEFILE_SOURCE',
  '-D_REENTRANT',
  '-D_FILE_OFFSET_BITS=64',
  '-DG_DISABLE_ASSERT',
  '-DG_DISABLE_CHECKS',
  '-DG_DISABLE_CAST_CHECKS',
  '-Wall',
  '-Wno-deprecated-declarations',
  '-Werror-implicit-function-declaration',
]

libfm_includes = include_directories('libfm', 'libfm/base', 'libfm/job', 'libfm/gtk','libfm/extra')

gtk = dependency('gtk+-3.0')
x11 = dependency('x11')
layershell = dependency('gtk-layer-shell-0')
menucache = dependency('libmenu-cache')

math = meson.get_compiler('c').find_library('m', required : false)

executable('pcmanfm',
  pcmanfm_sources + libfm_sources + libfm_gtk_sources,
  c_args : path_c_args + extra_c_args,
  include_directories : [ config_h_dir, libfm_includes ],
  dependencies : [ gtk, x11, layershell, menucache, math ],
  install : true,
)
