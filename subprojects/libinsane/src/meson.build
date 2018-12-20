pkg = import('pkgconfig')

libinsane_srcs = files(
    'bases/dumb.c',
    'basewrapper.c',
    'bmp.c',
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
    'util.c',
    'workarounds/cache.c',
    'workarounds/check_capabilities.c',
    'workarounds/dedicated_thread.c',
    'workarounds/one_page_flatbed.c',
    'workarounds/opt_names.c',
    'workarounds/opt_values.c',
    'workarounds/set_opt_late.c',
)

deps = [dependency('threads')]
extra_cflags = []


if host_machine.system() == 'windows'
    libinsane_srcs += [
        'bases/twain.c',
        'bases/wia/properties.c',
        'bases/wia/transfer.c',
        'bases/wia/util.c',
        'bases/wia/wia_ll.c',
    ]
    deps += [
        dependency('regex'),
        dependency('iconv'),
    ]
    # Recent versions of Meson (MXE for instance) allows this
    pkg_deps = deps
    pkg_cflags = []

    # Bug with Mingw32 + gcc 7.3.0:
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991
    extra_cflags += ['-mno-ms-bitfields']
else
    libinsane_srcs += [
        'bases/sane.c',
    ]
    deps += [dependency('sane-backends')]
    # Older versions of Meson only allow strings (Debian stretch for instance)
    pkg_deps = ['-pthreads', '-lsane']
    pkg_cflags = ['-pthreads']
endif

gen_version = custom_target(
    'gen-version-file',
    input: [],
    output: ['version.h'],
    command: [
        'sh', join_paths(meson.current_source_dir(), 'gen_version_file.sh'),
        '@OUTPUT@',
    ],
)


libinsane = library(
    'insane', libinsane_srcs, gen_version,
    include_directories: libinsane_inc,
    dependencies: deps,
    install: true,
    c_args: extra_cflags,
)
libinsane_dep = declare_dependency(
    include_directories: libinsane_inc,
    link_with: libinsane
)

pkg.generate(
    description: 'Cross-platform Cross-API Cross-driver Cross-scanner image scan library',
    name: 'libinsane',
    extra_cflags: pkg_cflags,
    libraries: [libinsane] + pkg_deps,
    version: '0.1',
)

