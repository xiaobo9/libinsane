if host_machine.system() != build_machine.system() or build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    LIBINSANE_TESTS = [
        'twain',
        'wia_ll',
    ]
else
    LIBINSANE_TESTS = [
        'sane',
    ]
endif

LIBINSANE_VALGRIND_TESTS = [
    'multiplexer',
    'normalizer_all_opts_on_all_sources',
    'normalizer_bmp2raw',
    'normalizer_clean_dev_descs',
    'normalizer_min_one_source',
    'normalizer_opt_aliases',
    'normalizer_raw24',
    'normalizer_resolution',
    'normalizer_safe_defaults',
    'normalizer_source_names',
    'normalizer_source_nodes',
    'normalizer_source_types',
    'workaround_cache',
    'workaround_check_capabilities',
    'workaround_set_opt_late',
    'workaround_dedicated_thread',
    'workaround_one_page_flatbed',
    'workaround_opt_names',
    'workaround_opt_values',
]

CUNIT = dependency('cunit')

foreach t: LIBINSANE_TESTS
    e = executable('tests_@0@'.format(t), 'main.c', 'tests_@0@.c'.format(t),
        dependencies: [libinsane_dep, CUNIT])
    test('tests_@0@'.format(t), e)
endforeach

if host_machine.system() != build_machine.system() or build_machine.system() == 'windows' or build_machine.system() == 'cygwin'

    foreach t: LIBINSANE_VALGRIND_TESTS
        e = executable(
            'tests_@0@'.format(t),
            'main.c',
            'tests_@0@.c'.format(t),
            dependencies: [libinsane_dep, CUNIT]
        )
        test('tests_@0@'.format(t), e)
    endforeach

else

    vg = find_program('valgrind')
    foreach t: LIBINSANE_VALGRIND_TESTS
        e = executable(
            'tests_@0@'.format(t),
            'main.c',
            'tests_@0@.c'.format(t),
            dependencies: [libinsane_dep, CUNIT]
        )
        test('tests_@0@'.format(t), vg,
            args: [
                '--trace-children=yes',
                '--leak-check=full',
                '--error-exitcode=10',
                e.full_path()
            ])
    endforeach

endif
