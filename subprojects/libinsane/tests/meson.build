LIBINSANE_TESTS = [
    'sane',
]

LIBINSANE_VALGRIND_TESTS = [
    'multiplexer',
    'normalizer_safe_defaults',
    'normalizer_source_nodes',
    'workaround_opt_mode',
    'workaround_opt_names',
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
