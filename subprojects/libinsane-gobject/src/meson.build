INSANE_GOBJECT_SRCS = files(
    'device_descriptor.c',
    'error.c',
    'item.c',
    'libinsane-api.c',
    'log.c',
    'option_descriptor.c',
    'scan_parameters.c',
    'scan_session.c',
)

GOBJECT = dependency('gobject-2.0')
libinsane_dep = LIBINSANE.get_variable('libinsane_dep')

## Generate enums.h / enums.c

enums = gnome.mkenums('enums',
    c_template: 'enums.c.template',
    h_template: 'enums.h.template',
    sources: INSANE_GOBJECT_HEADERS,
    install_header: true,
    install_dir: join_paths(get_option('includedir'), 'libinsane-gobject'))

## Build libinsane_gobject itself

libinsane_gobject = library('insane_gobject', INSANE_GOBJECT_SRCS + enums,
    include_directories: libinsane_gobject_inc,
    dependencies: [libinsane_dep, GOBJECT],
    version: meson.project_version(),
    install: true)
libinsane_gobject_dep = declare_dependency(
    include_directories: libinsane_gobject_inc,
    link_with: libinsane_gobject,
    dependencies: [GOBJECT])

## Introspection

gir = gnome.generate_gir(libinsane_gobject,
    sources: INSANE_GOBJECT_SRCS + INSANE_GOBJECT_HEADERS,
    namespace: 'Libinsane',
    # Not a typo: This is the GObject API version. Not the Libinsane
    # version.
    nsversion: '1.0',
    includes: 'GObject-2.0',
    install: true)

## Vala support

gnome.generate_vapi('libinsane',
    sources: [gir.get(0)],
    install: true,
    packages: ['glib-2.0', 'gobject-2.0'])
