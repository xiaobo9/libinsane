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
static struct lis_api *g_res = NULL;


static void tests_resolution_integer_range(void)
{
	static const struct lis_option_descriptor opt_resolution = {
		.name = OPT_NAME_RESOLUTION,
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_DPI,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible.range = {
				.min.integer = 50,
				.max.integer = 250,
				.interval.integer = 50,
			},
		},
	};
	static const union lis_value opt_resolution_default = {
		.integer = 120,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;

	g_res = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_resolution, &opt_resolution_default);

	err = lis_api_normalizer_resolution(g_dumb, &g_res);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_res->get_device(g_res, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->value.type, LIS_TYPE_INTEGER);
	LIS_ASSERT_EQUAL(opts[0]->constraint.type, LIS_CONSTRAINT_LIST);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.nb_values, 5);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[0].integer, 50);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[1].integer, 100);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[2].integer, 150);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[3].integer, 200);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[4].integer, 250);

	item->close(item);
	g_res->cleanup(g_res);
}


static void tests_resolution_double_list(void)
{
	static union lis_value resolutions[] = {
		{ .dbl = 50.0 },
		{ .dbl = 100.0 },
		{ .dbl = 150.0 },
		{ .dbl = 200.0 },
		{ .dbl = 250.0 },
	};
	static const struct lis_option_descriptor opt_resolution = {
		.name = OPT_NAME_RESOLUTION,
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_DOUBLE,
			.unit = LIS_UNIT_DPI,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.values = resolutions,
				.nb_values = LIS_COUNT_OF(resolutions),
			},
		},
	};
	static const union lis_value opt_resolution_default = {
		.integer = 120,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;

	g_res = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_resolution, &opt_resolution_default);

	err = lis_api_normalizer_resolution(g_dumb, &g_res);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_res->get_device(g_res, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->value.type, LIS_TYPE_INTEGER);
	LIS_ASSERT_EQUAL(opts[0]->constraint.type, LIS_CONSTRAINT_LIST);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.nb_values, 5);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[0].integer, 50);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[1].integer, 100);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[2].integer, 150);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[3].integer, 200);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[4].integer, 250);

	item->close(item);
	g_res->cleanup(g_res);
}


static void tests_resolution_double_range(void)
{
	static const struct lis_option_descriptor opt_resolution = {
		.name = OPT_NAME_RESOLUTION,
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_DOUBLE,
			.unit = LIS_UNIT_DPI,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible.range = {
				.min.dbl = 50.0,
				.max.dbl = 250.0,
				.interval.dbl = 50.0,
			},
		},
	};
	static const union lis_value opt_resolution_default = {
		.dbl = 120.0,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;

	g_res = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_resolution, &opt_resolution_default);

	err = lis_api_normalizer_resolution(g_dumb, &g_res);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_res->get_device(g_res, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->value.type, LIS_TYPE_INTEGER);
	LIS_ASSERT_EQUAL(opts[0]->constraint.type, LIS_CONSTRAINT_LIST);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.nb_values, 5);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[0].integer, 50);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[1].integer, 100);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[2].integer, 150);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[3].integer, 200);
	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.values[4].integer, 250);

	item->close(item);
	g_res->cleanup(g_res);
}


static void tests_resolution_double_range_getset(void)
{
	static const struct lis_option_descriptor opt_resolution = {
		.name = OPT_NAME_RESOLUTION,
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_DOUBLE,
			.unit = LIS_UNIT_DPI,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible.range = {
				.min.dbl = 50.0,
				.max.dbl = 250.0,
				.interval.dbl = 50.0,
			},
		},
	};
	static const union lis_value opt_resolution_default = {
		.dbl = 120.0,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	enum lis_error err;
	int set_flags;

	g_res = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_resolution, &opt_resolution_default);

	err = lis_api_normalizer_resolution(g_dumb, &g_res);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_res->get_device(g_res, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->value.type, LIS_TYPE_INTEGER);
	LIS_ASSERT_EQUAL(opts[0]->constraint.type, LIS_CONSTRAINT_LIST);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 120);

	value.integer = 200;
	err = opts[0]->fn.set_value(opts[0], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);

	item->close(item);
	g_res->cleanup(g_res);
}

int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("opt_mode", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_resolution_integer_range()", tests_resolution_integer_range) == NULL
			|| CU_add_test(suite, "tests_resolution_double_list()",
				tests_resolution_double_list) == NULL
			|| CU_add_test(suite, "tests_resolution_double_range()",
				tests_resolution_double_range) == NULL
			|| CU_add_test(suite, "tests_resolution_double_range_getset()",
				tests_resolution_double_range_getset) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
