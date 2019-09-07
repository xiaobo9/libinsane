#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_opt = NULL;


static int tests_opt_init(int add_opt_resolution)
{
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED, },
		{ .string = OPT_VALUE_SOURCE_ADF, },
	};
	static const struct lis_option_descriptor opt_source_template = {
		.name = "doc-source",
		.title = "source title",
		.desc = "source desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_STRING,
			.unit = LIS_UNIT_NONE,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.nb_values = LIS_COUNT_OF(opt_source_constraint),
				.values = (union lis_value*)&opt_source_constraint,
			},
		},
	};
	static const union lis_value opt_source_default = {
		.string = OPT_VALUE_SOURCE_FLATBED,
	};
	static const struct lis_option_descriptor opt_scan_resolution_template = {
		.name = "scan-resolution",
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_NONE,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_NONE,
		},
	};
	static const struct lis_option_descriptor opt_resolution_template = {
		.name = OPT_NAME_RESOLUTION,
		.title = "resolution title",
		.desc = "resolution desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_NONE,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_NONE,
		},
	};
	static const union lis_value opt_resolution_default = {
		.integer = 300,
	};
	enum lis_error err;

	g_opt = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(
		g_dumb, &opt_source_template, &opt_source_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_scan_resolution_template, &opt_resolution_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	if (add_opt_resolution) {
		lis_dumb_add_option(
			g_dumb, &opt_resolution_template, &opt_resolution_default,
			LIS_SET_FLAG_MUST_RELOAD_PARAMS
		);
	}

	return 0;
}


static int tests_opt_clean(void)
{
	struct lis_api *api = (g_opt != NULL ? g_opt : g_dumb);
	api->cleanup(api);
	return 0;
}

static void tests_opt_names(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL;
	struct lis_option_descriptor **opt_opts = NULL;

	LIS_ASSERT_EQUAL(tests_opt_init(0 /* add_opt_resolution */), 0);

	err = lis_api_workaround_opt_names(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[0]->name, OPT_NAME_SOURCE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[1]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opt_opts[2], NULL);

	opt_item->close(opt_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


static void tests_opt_with_both(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL;
	struct lis_option_descriptor **opt_opts = NULL;

	LIS_ASSERT_EQUAL(tests_opt_init(1 /* add_opt_resolution */), 0);

	err = lis_api_workaround_opt_names(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[0]->name, OPT_NAME_SOURCE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[1]->name, "scan-resolution"), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[2]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opt_opts[3], NULL);

	opt_item->close(opt_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("opt_names", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_opt_names()", tests_opt_names) == NULL
			|| CU_add_test(suite, "tests_opt_with_both()", tests_opt_with_both) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
