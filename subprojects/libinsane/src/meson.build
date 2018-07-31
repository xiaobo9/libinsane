LIBINSANE_SRCS = files(
    'bases/dumb.c',
    'basewrapper.c',
    'error.c',
    'log.c',
    'multiplexer.c',
    'normalizers/all_opts_on_all_sources.c',
    'normalizers/end_of_feed.c',
    'normalizers/flatbed_and_feeder_behavior.c',
    'normalizers/min_one_source.c',
    'normalizers/raw.c',
    'normalizers/raw24.c',
    'normalizers/resolution_format.c',
    'normalizers/safe_defaults.c',
    'normalizers/scan_area_opts.c',
    'normalizers/source_names.c',
    'normalizers/source_nodes.c',
    'normalizers/source_types.c',
    'normalizers/sources_order.c',
    'normalizers/strip_non_scanners.c',
    'safebet.c',
    'str2impls.c',
    'workarounds/clean_dev_model_char.c',
    'workarounds/clean_dev_model_from_manufacturer.c',
    'workarounds/dedicated_thread.c',
    'workarounds/no_read_on_inactive_opt.c',
    'workarounds/no_write_on_readonly_opt.c',
    'workarounds/opt_mode.c',
    'workarounds/opt_names.c',
    'workarounds/opts_page_size.c',
    'workarounds/strip_translations.c',
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
