#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_src = NULL;
static struct lis_api *g_check = NULL;


static int tests_init(void)
{
	static const struct lis_option_descriptor opt_resolution_inactive = {
		.name = OPT_NAME_RESOLUTION"_inactive",
		.title = "resolution inactive title",
		.desc = "resolution inactive desc",
		.capabilities = LIS_CAP_INACTIVE | LIS_CAP_SW_SELECT,
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
	static const union lis_value opt_resolution_inactive_default = {
		.integer = 120,
	};
	static const struct lis_option_descriptor opt_resolution_readonly = {
		.name = OPT_NAME_RESOLUTION"_readonly",
		.title = "resolution read-only title",
		.desc = "resolution read-only desc",
		.capabilities = 0, /* read-only */
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
	static const union lis_value opt_resolution_readonly_default = {
		.integer = 160,
	};
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED },
	};
	static const struct lis_option_descriptor opt_source = {
		.name = OPT_NAME_SOURCE,
		.title = "source title",
		.desc = "source desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_DPI,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.nb_values = LIS_COUNT_OF(opt_source_constraint),
				.values = (union lis_value *)&opt_source_constraint,
			},
		},
	};
	static const union lis_value opt_source_default = {
		.string = OPT_VALUE_SOURCE_FLATBED,
	};
	enum lis_error err;

	g_check = NULL;
	g_dumb = NULL;
	g_src = NULL;

	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(
		g_dumb, &opt_resolution_inactive, &opt_resolution_inactive_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_resolution_readonly, &opt_resolution_readonly_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_source, &opt_source_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);

	err = lis_api_normalizer_min_one_source(g_dumb, &g_src);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}
	return 0;
}


static int tests_cleanup(void)
{
	struct lis_api *api = (g_check ? g_check : (g_src ? g_src : g_dumb));
	api->cleanup(api);
	return 0;
}


static void tests_inactive(void)
{
	enum lis_error err;
	struct lis_item *item;
	struct lis_item **children;
	struct lis_option_descriptor **opts;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_init(), 0);

	err = lis_api_workaround_check_capabilities(g_src, &g_check);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_check->get_device(g_check, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_EQUAL(children[1], NULL);

	err = children[0]->get_options(children[0], &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_RESOLUTION"_inactive"), 0);
	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_ERR_ACCESS_DENIED);
	value.integer = 200;
	err = opts[0]->fn.set_value(opts[0], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_ERR_ACCESS_DENIED);

	item->close(item);
	LIS_ASSERT_EQUAL(tests_cleanup(), 0);
}


static void tests_read_only(void)
{
	enum lis_error err;

	struct lis_item *item;
	struct lis_option_descriptor **opts;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_init(), 0);

	err = lis_api_workaround_check_capabilities(g_src, &g_check);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_check->get_device(g_check, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[1]->name, OPT_NAME_RESOLUTION"_readonly"), 0);
	err = opts[1]->fn.get_value(opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 160);
	value.integer = 200;
	err = opts[1]->fn.set_value(opts[1], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_ERR_ACCESS_DENIED);

	item->close(item);
	LIS_ASSERT_EQUAL(tests_cleanup(), 0);
}


static void tests_single_value(void)
{
	enum lis_error err;
	struct lis_item *item;
	struct lis_option_descriptor **opts;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_init(), 0);

	err = lis_api_workaround_check_capabilities(g_src, &g_check);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_check->get_device(g_check, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[2]->name, OPT_NAME_SOURCE), 0);
	err = opts[2]->fn.get_value(opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_FLATBED), 0);

	value.string = OPT_VALUE_SOURCE_FLATBED;
	err = opts[2]->fn.set_value(opts[2], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	value.string = OPT_VALUE_SOURCE_ADF;
	err = opts[2]->fn.set_value(opts[2], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_ERR_INVALID_VALUE);

	item->close(item);
	LIS_ASSERT_EQUAL(tests_cleanup(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("check_capabilities", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "inactive", tests_inactive) == NULL
			|| CU_add_test(suite, "read_only", tests_read_only) == NULL
			|| CU_add_test(suite, "single_value", tests_single_value) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
