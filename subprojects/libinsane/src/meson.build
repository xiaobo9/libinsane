LIBINSANE_SRCS = files(
    'bases/dumb.c',
    'basewrapper.c',
    'error.c',
    'log.c',
    'multiplexer.c',
    'normalizers/all_opts_on_all_sources.c',
    'normalizers/bmp2raw.c',
    'normalizers/clean_dev_descs.c',
    'normalizers/min_one_source.c',
    'normalizers/opt_aliases.c',
    'normalizers/raw24.c',
    'normalizers/resolution.c',
    'normalizers/safe_defaults.c',
    'normalizers/source_names.c',
    'normalizers/source_nodes.c',
    'normalizers/source_types.c',
    'safebet.c',
    'str2impls.c',
    'workarounds/dedicated_thread.c',
    'workarounds/check_capabilities.c',
    'workarounds/opt_names.c',
    'workarounds/opt_values.c',
    'util.c',
)

if host_machine.system() == 'windows'
    LIBINSANE_SRCS += [
        'bases/twain.c',
        'bases/wia_automation.c',
        'bases/wia_ll.c',
    ]
    SANE = []
else
    LIBINSANE_SRCS += [
        'bases/sane.c',
    ]
    SANE = dependency('sane-backends')
endif

libinsane = library('insane', LIBINSANE_SRCS,
    include_directories: libinsane_inc,
    dependencies: SANE,
    install: true)
libinsane_dep = declare_dependency(
    include_directories: libinsane_inc,
    link_with: libinsane)
