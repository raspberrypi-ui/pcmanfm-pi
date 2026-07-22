subdir('ui')

install_data('libfm.xml', install_dir: get_option('datadir') / 'mime' / 'packages')

install_data('libfm.conf', install_dir: get_option('sysconfdir') / 'xdg' / 'libfm')

install_data('archivers.list', 'terminals.list',
  install_dir: get_option('datadir') / 'libfm',
)

install_data('images/unknown.png', 'images/folder.png',
  install_dir: get_option('datadir') / 'libfm' / 'images',
)
