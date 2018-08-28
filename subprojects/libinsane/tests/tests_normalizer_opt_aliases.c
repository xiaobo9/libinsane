#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_alias = NULL;


static void tests_alias_xres(void)
{
	static const struct lis_option_descriptor opt_xres = {
		.name = "xres",
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
	static const union lis_value opt_xres_default = {
		.integer = 120,
	};
	static const struct lis_option_descriptor opt_yres = {
		.name = "yres",
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
	static const union lis_value opt_yres_default = {
		.integer = 180,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;
	union lis_value value;
	int set_flags;

	g_alias = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_xres, &opt_xres_default);
	lis_dumb_add_option(g_dumb, &opt_yres, &opt_yres_default);

	err = lis_api_normalizer_opt_aliases(g_dumb, &g_alias);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_alias->get_device(g_alias, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, "xres"), 0);
	LIS_ASSERT_EQUAL(strcmp(opts[1]->name, "yres"), 0);
	LIS_ASSERT_EQUAL(strcmp(opts[2]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[3], NULL);

	err = opts[2]->fn.get_value(opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 120);

	value.integer = 200;
	err = opts[2]->fn.set_value(opts[2], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opts[2]->fn.get_value(opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);
	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);
	err = opts[1]->fn.get_value(opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);

	item->close(item);
	g_alias->cleanup(g_alias);
}


static void tests_xpos_xextent(void)
{
	static const struct lis_option_descriptor opt_xpos = {
		.name = "xpos",
		.title = "meh",
		.desc = "bleh",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_PIXEL,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible.range = {
				.min.integer = 0,
				.max.integer = 250,
				.interval.integer = 1,
			},
		},
	};
	static const union lis_value opt_xpos_default = {
		.integer = 120,
	};
	static const struct lis_option_descriptor opt_xextent = {
		.name = "xextent",
		.title = "meh2",
		.desc = "bleh2",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_PIXEL,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible.range = {
				.min.integer = 50,
				.max.integer = 250,
				.interval.integer = 1,
			},
		},
	};
	static const union lis_value opt_xextent_default = {
		.integer = 50,
	};
	struct lis_item *item = NULL;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;
	union lis_value value;
	int set_flags;

	g_alias = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	LIS_ASSERT_EQUAL(err, LIS_OK);

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_xpos, &opt_xpos_default);
	lis_dumb_add_option(g_dumb, &opt_xextent, &opt_xextent_default);

	err = lis_api_normalizer_opt_aliases(g_dumb, &g_alias);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_alias->get_device(g_alias, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, "xpos"), 0);
	LIS_ASSERT_EQUAL(strcmp(opts[1]->name, "xextent"), 0);
	LIS_ASSERT_EQUAL(strcmp(opts[2]->name, OPT_NAME_TL_X), 0);
	LIS_ASSERT_EQUAL(strcmp(opts[3]->name, OPT_NAME_BR_X), 0);
	LIS_ASSERT_EQUAL(opts[4], NULL);

	err = opts[2]->fn.get_value(opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 120);
	err = opts[3]->fn.get_value(opts[3], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 170);

	value.integer = 100;
	err = opts[2]->fn.set_value(opts[2], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opts[2]->fn.get_value(opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 100);
	err = opts[3]->fn.get_value(opts[3], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 170);
	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 100);
	err = opts[1]->fn.get_value(opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 70);

	item->close(item);
	g_alias->cleanup(g_alias);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("opt_mode", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_alias_xres()", tests_alias_xres) == NULL
			|| CU_add_test(suite, "tests_xpos_xextent()", tests_xpos_xextent) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
