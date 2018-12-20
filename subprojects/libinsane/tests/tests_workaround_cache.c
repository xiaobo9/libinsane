#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/dumb.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"


static int tests_cache_init(void)
{
	// TODO
	return 0;
}


static int tests_cache_cleanup(void)
{
	// TODO
	return 0;
}


static void test_cache_list_options(void)
{
	LIS_ASSERT_EQUAL(tests_cache_init(), 0);
	// TODO
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_get_value(void)
{
	LIS_ASSERT_EQUAL(tests_cache_init(), 0);
	// TODO
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_set_value(void)
{
	LIS_ASSERT_EQUAL(tests_cache_init(), 0);
	// TODO
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("cache", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "list_options", test_cache_list_options) == NULL
			|| CU_add_test(suite, "get_value", test_cache_get_value) == NULL
			|| CU_add_test(suite, "set_value", test_cache_set_value) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}

