ui_sources = [
  'pref.glade',
  'desktop-pref.glade',
  'autorun.glade',
  'connect.glade',
]

libfm_ui_sources = [
  'file-prop.glade',
  'progress.glade',
  'ask-rename.glade',
  'app-chooser.glade',
  'exec-file.glade',
  'filesearch.glade',
  'choose-icon.glade',
]

about_conf = configuration_data()
about_conf.set('VERSION', meson.project_version())

about_glade = configure_file(
  input : 'about.glade.in',
  output : 'about.glade',
  configuration : about_conf,
)

glade_to_ui = files('glade2ui.sh')

foreach f : ui_sources + libfm_ui_sources
  out = f.replace('.glade', '.ui')
  custom_target(out,
    input : f,
    output : out,
    command : [glade_to_ui, '@INPUT@', '@OUTPUT@'],
    install : true,
    install_dir : uidir,
  )
endforeach

custom_target('about.ui',
  input : about_glade,
  output : 'about.ui',
  command : [glade_to_ui, '@INPUT@', '@OUTPUT@'],
  install : true,
  install_dir : uidir,
)
