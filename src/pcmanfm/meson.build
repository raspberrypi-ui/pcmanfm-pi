pcmanfm_sources = files(
  'pcmanfm.c',
  'app-config.c',
  'main-win.c',
  'tab-page.c',
  'desktop.c',
  'volume-manager.c',
  'pref.c',
  'single-inst.c',
  'connect-server.c',
)

pcmanfm_c_args = [
  '-DPACKAGE_DATA_DIR="' + pcmanfm_datadir_abs + '"',
  '-DPACKAGE_UI_DIR="' + pcmanfm_uidir_abs + '"',
  '-DPACKAGE_LOCALE_DIR="' + pcmanfm_localedir_abs + '"',
  '-Wall',
  '-Werror-implicit-function-declaration',
] + g_cast_checks_args

executable('pcmanfm',
  pcmanfm_sources,
  c_args : pcmanfm_c_args,
  include_directories : [config_h_dir, inc_top, inc_common, inc_gtk],
  dependencies : pcmanfm_deps + [x11_dep],
  link_with : [libfm_lib, libfm_gtk_lib],
  install : true,
)

install_headers('pcmanfm-modules.h')

# prepare empty modules directory, matching the autotools install-exec-local
install_emptydir(pcmanfm_modules_dir)
