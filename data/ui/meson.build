# GtkBuilder UI definition files
ui_sources = [
  'pref.glade',
  'desktop-pref.glade',
  'autorun.glade',
  'connect.glade',
]

# libfm's own GtkBuilder UI files; libfm's PACKAGE_UI_DIR is now pcmanfm's
# own ui_install_dir too (no separate datadir/libfm), and none of these
# filenames collide with pcmanfm's own ui_sources above.
libfm_ui_sources = [
  'file-prop.glade',
  'progress.glade',
  'ask-rename.glade',
  'app-chooser.glade',
  'exec-file.glade',
  'filesearch.glade',
  'choose-icon.glade',
]

ui_install_dir = get_option('datadir') / 'pcmanfm' / 'ui'

about_conf = configuration_data()
about_conf.set('VERSION', meson.project_version())

about_glade = configure_file(
  input : 'about.glade.in',
  output : 'about.glade',
  configuration : about_conf,
)

# Purge GtkBuilder UI files: strip comments and collapse inter-tag whitespace
glade_to_ui = files('glade2ui.sh')

foreach f : ui_sources + libfm_ui_sources
  out = f.replace('.glade', '.ui')
  custom_target(out,
    input : f,
    output : out,
    command : [glade_to_ui, '@INPUT@', '@OUTPUT@'],
    install : true,
    install_dir : ui_install_dir,
  )
endforeach

custom_target('about.ui',
  input : about_glade,
  output : 'about.ui',
  command : [glade_to_ui, '@INPUT@', '@OUTPUT@'],
  install : true,
  install_dir : ui_install_dir,
)
