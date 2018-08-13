LIBINSANE_TESTS = [
    'sane',
]

LIBINSANE_VALGRIND_TESTS = [
    'multiplexer',
    'normalizer_clean_dev_descs',
    'normalizer_min_one_source',
    'normalizer_resolution',
    'normalizer_safe_defaults',
    'normalizer_source_names',
    'normalizer_source_nodes',
    'normalizer_source_types',
    'workaround_opt_values',
    'workaround_opt_names',
    'workaround_check_capabilities',
]

CUNIT = dependency('cunit')
vg = find_program('valgrind')

foreach t: LIBINSANE_TESTS
    e = executable('tests_@0@'.format(t), 'main.c', 'tests_@0@.c'.format(t),
        dependencies: [libinsane_dep, CUNIT])
    test('tests_@0@'.format(t), e)
endforeach
foreach t: LIBINSANE_VALGRIND_TESTS
    e = executable('tests_@0@'.format(t), 'main.c', 'tests_@0@.c'.format(t),
        dependencies: [libinsane_dep, CUNIT])
    test('tests_@0@'.format(t), vg,
        args: ['--trace-children=yes', '--leak-check=full', '--error-exitcode=10', e.full_path()])
endforeach
