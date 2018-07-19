doxygen = find_program('doxygen')

cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
cdata.set('TOP_SRCDIR', join_paths([meson.current_source_dir(), '..']))

doxyfile = configure_file(input: 'doxygen.conf.in',
                          output: 'doxygen.conf',
                          configuration: cdata,
                          install: false)

datadir = join_paths(get_option('datadir'), 'doc')

doc_target = custom_target(
    'doc',
    input: doxyfile,
    output: 'doc_out',
    command: [doxygen, doxyfile],
    install: false,
)
