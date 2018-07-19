LIBINSANE_HEADERS = files(
    'libinsane/capi.h',
    'libinsane/dumb.h',
    'libinsane/error.h',
    'libinsane/log.h',
    'libinsane/multiplexer.h',
    'libinsane/normalizers.h',
    'libinsane/safebet.h',
    'libinsane/str2impls.h',
    'libinsane/util.h',
    'libinsane/workarounds.h',
)

if host_machine.system() == 'windows'
    LIBINSANE_HEADERS += [
        'libinsane/twain.h',
        'libinsane/wia_ll.h',
        'libinsane/wia_automation.h',
    ]
else
    LIBINSANE_HEADERS += [
        'libinsane/sane.h',
    ]
endif

install_headers(LIBINSANE_HEADERS, subdir: 'libinsane')
libinsane_inc = include_directories('.')
