man_conf = configuration_data()
man_conf.set('PACKAGE', meson.project_name())
man_conf.set('VERSION', meson.project_version())

configure_file(
  input : 'pcmanfm.1.in',
  output : 'pcmanfm.1',
  configuration : man_conf,
  install : true,
  install_dir : get_option('mandir') / 'man1',
)

i18n.merge_file(
  input : 'pcmanfm.desktop.in',
  output : 'pcmanfm.desktop',
  type : 'desktop',
  po_dir : '../po',
  install : true,
  install_dir : get_option('datadir') / 'applications',
)

install_data('pcmanfm.conf',
  install_dir : get_option('sysconfdir') / 'xdg' / 'pcmanfm' / 'default',
)

# libfm's own data files. libfm.xml (mime registration) and libfm.conf
# (XDG config lookup) use their own historical locations, unrelated to
# PACKAGE_DATA_DIR. archivers.list/terminals.list/images are looked up via
# libfm's PACKAGE_DATA_DIR/PACKAGE_THEME_DIR, which are now pcmanfm's own
# shared dirs (see root meson.build), so they install there too.
install_data('libfm.xml',
  install_dir : get_option('datadir') / 'mime' / 'packages',
)

install_data('libfm.conf',
  install_dir : get_option('sysconfdir') / 'xdg' / 'libfm',
)

install_data('archivers.list', 'terminals.list',
  install_dir : get_option('datadir') / 'pcmanfm',
)

install_data('images/unknown.png', 'images/folder.png',
  install_dir : get_option('datadir') / 'pcmanfm' / 'images',
)

subdir('ui')
subdir('icons')
