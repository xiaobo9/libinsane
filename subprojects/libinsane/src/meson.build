libinsane_srcs = files(
    'bases/dumb.c',
    'basewrapper.c',
    'error.c',
    'log.c',
    'multiplexer.c',
    'normalizers/all_opts_on_all_sources.c',
    'normalizers/bmp.c',
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
    'workarounds/one_page_flatbed.c',
    'workarounds/opt_names.c',
    'workarounds/opt_values.c',
    'util.c',
)

deps = [dependency('threads')]

if host_machine.system() == 'windows'
    libinsane_srcs += [
        'bases/twain.c',
        'bases/wia_automation.c',
        'bases/wia_ll.c',
    ]
else
    libinsane_srcs += [
        'bases/sane.c',
    ]
    deps += dependency('sane-backends')
endif

libinsane = library('insane', libinsane_srcs,
    include_directories: libinsane_inc,
    dependencies: deps,
    install: true)
libinsane_dep = declare_dependency(
    include_directories: libinsane_inc,
    link_with: libinsane)
