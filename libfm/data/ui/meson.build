glade_files = [
  'file-prop.glade',
  'progress.glade',
  'ask-rename.glade',
  'app-chooser.glade',
  'exec-file.glade',
  'filesearch.glade',
]

strip_glade_comments = find_program('strip-glade-comments.sh')
uidir = get_option('datadir') / 'libfm' / 'ui'

foreach glade : glade_files
  custom_target(glade.replace('.glade', '.ui'),
    input: glade,
    output: glade.replace('.glade', '.ui'),
    command: [strip_glade_comments, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: uidir,
  )
endforeach

install_data('choose-icon.ui', install_dir: uidir)
