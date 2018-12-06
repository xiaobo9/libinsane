#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_dd = NULL;

static int tests_dd_init(void)
{
	enum lis_error err;

	g_dumb = NULL;
	g_dd = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_dd_clean(void)
{
	struct lis_api *api = (g_dd != NULL ? g_dd : g_dumb);
	api->cleanup(api);
	return 0;
}


static void tests_clean_underscores(void)
{
	static struct lis_device_descriptor dev = {
		.dev_id = LIS_DUMB_DEV_ID_FIRST,
		.vendor = "ABC_DEF",
		.model = "super_scanner_top_moumoutte",
		.type = "camion",
	};
	static struct lis_device_descriptor *devs[] = { &dev, NULL };
	enum lis_error err;
	struct lis_device_descriptor **out;

	LIS_ASSERT_EQUAL(tests_dd_init(), 0);

	lis_dumb_set_dev_descs(g_dumb, devs);

	err = lis_api_normalizer_clean_dev_descs(g_dumb, &g_dd);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_dd->list_devices(g_dd, LIS_DEVICE_LOCATIONS_ANY, &out);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(out[0]->vendor, "ABC DEF"), 0);
	LIS_ASSERT_EQUAL(strcmp(out[0]->model, "super scanner top moumoutte"), 0);
	LIS_ASSERT_EQUAL(out[1], NULL);

	LIS_ASSERT_EQUAL(tests_dd_clean(), 0);
}


static void tests_clean_manufacturer(void)
{
	static struct lis_device_descriptor dev = {
		.dev_id = LIS_DUMB_DEV_ID_FIRST,
		.vendor = "BUILDER",
		.model = "BUILDER MODEL NUM",
		.type = "camion",
	};
	static struct lis_device_descriptor *devs[] = { &dev, NULL };
	enum lis_error err;
	struct lis_device_descriptor **out;

	LIS_ASSERT_EQUAL(tests_dd_init(), 0);

	lis_dumb_set_dev_descs(g_dumb, devs);

	err = lis_api_normalizer_clean_dev_descs(g_dumb, &g_dd);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_dd->list_devices(g_dd, LIS_DEVICE_LOCATIONS_ANY, &out);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(out[0]->vendor, "BUILDER"), 0);
	LIS_ASSERT_EQUAL(strcmp(out[0]->model, "MODEL NUM"), 0);
	LIS_ASSERT_EQUAL(out[1], NULL);

	LIS_ASSERT_EQUAL(tests_dd_clean(), 0);
}


static void tests_clean_long_manufacturer_names(void)
{
	static struct lis_device_descriptor dev = {
		.dev_id = LIS_DUMB_DEV_ID_FIRST,
		.vendor = "hewleTt-pAckard",
		.model = "HP MODEL NUM",
		.type = "camion",
	};
	static struct lis_device_descriptor *devs[] = { &dev, NULL };
	enum lis_error err;
	struct lis_device_descriptor **out;

	LIS_ASSERT_EQUAL(tests_dd_init(), 0);

	lis_dumb_set_dev_descs(g_dumb, devs);

	err = lis_api_normalizer_clean_dev_descs(g_dumb, &g_dd);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_dd->list_devices(g_dd, LIS_DEVICE_LOCATIONS_ANY, &out);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(out[0]->vendor, "HP"), 0);
	LIS_ASSERT_EQUAL(strcmp(out[0]->model, "MODEL NUM"), 0);
	LIS_ASSERT_EQUAL(out[1], NULL);

	LIS_ASSERT_EQUAL(tests_dd_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("normalizer_clean_dev_descs", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_clean_underscores()", tests_clean_underscores) == NULL
			|| CU_add_test(suite, "tests_clean_manufacturer()",
				tests_clean_manufacturer) == NULL
			|| CU_add_test(suite, "tests_clean_long_manufacturer_names()",
				tests_clean_long_manufacturer_names) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
