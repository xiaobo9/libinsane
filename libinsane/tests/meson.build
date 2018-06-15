LIBINSANE_TESTS = [
    'sane',
]

LIBINSANE_VALGRIND_TESTS = [
    'multiplexer',
]

CUNIT = dependency('cunit')
vg = find_program('valgrind')

foreach t: LIBINSANE_TESTS
    e = executable('tests_@0@'.format(t), 'main.c', 'tests_@0@.c'.format(t),
        dependencies: [libinsane_dep, CUNIT])
    test('tests_@0@'.format(t), e, env: ['CTEST_OUTPUT_ON_FAILURE=1'])
endforeach
foreach t: LIBINSANE_VALGRIND_TESTS
    e = executable('tests_@0@'.format(t), 'main.c', 'tests_@0@.c'.format(t),
        dependencies: [libinsane_dep, CUNIT])
    test('tests_@0@'.format(t), vg,
        args: ['--trace-children=yes', '--leak-check=full', '--error-exitcode=10', e],
        env: ['CTEST_OUTPUT_ON_FAILURE=1'])
endforeach
