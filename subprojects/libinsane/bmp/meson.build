bmp_parser_srcs = files('bmp_parser.c')

extra_cflags = []

if host_machine.system() == 'windows'
    # Bug with Mingw32 + gcc 7.3.0:
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991
    extra_cflags += ['-mno-ms-bitfields']
endif

executable(
    'bmp_parser',
    bmp_parser_srcs,
    include_directories: libinsane_inc,
    install: false,
    c_args: extra_cflags,
)
