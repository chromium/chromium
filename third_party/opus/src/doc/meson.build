have_dot = find_program('dot', required: false).found()

doxyfile_conf = configuration_data()
doxyfile_conf.set('VERSION', opus_version)
doxyfile_conf.set('HAVE_DOT', have_dot)
doxyfile_conf.set('top_srcdir', top_srcdir)
doxyfile_conf.set('top_builddir', top_builddir)

doxyfile = configure_file(input: 'Doxyfile.in',
  output: 'Doxyfile',
  configuration: doxyfile_conf,
  install: false)

docdir = join_paths(get_option('datadir'), get_option('docdir'))

doc_inputs = [
  'customdoxygen.css',
  'footer.html',
  'header.html',
  'opus_logo.svg',
  top_srcdir + '/include/opus.h',
  top_srcdir + '/include/opus_multistream.h',
  top_srcdir + '/include/opus_defines.h',
  top_srcdir + '/include/opus_types.h',
  top_srcdir + '/include/opus_custom.h',
]

custom_target('doc',
  input: [ doxyfile ] + doc_inputs,
  output: [ 'html' ],
  command: [ doxygen, doxyfile ],
  install_dir: docdir,
  install: true)
