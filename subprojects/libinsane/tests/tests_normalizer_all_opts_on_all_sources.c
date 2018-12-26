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
static struct lis_api *g_sn = NULL;
static struct lis_api *g_opts = NULL;


static int tests_opts_init(void)
{
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED, },
		{ .string = OPT_VALUE_SOURCE_ADF, },
	};
	static const struct lis_option_descriptor opt_source_template = {
		.name = OPT_NAME_SOURCE,
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
		.string = OPT_VALUE_SOURCE_FLATBED
	};
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
	enum lis_error err;

	g_dumb = NULL;
	g_sn = NULL;
	g_opts = NULL;
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
		g_dumb, &opt_resolution, &opt_resolution_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_opts_clean(void)
{
	struct lis_api *api = (g_opts != NULL ? g_opts : (g_sn != NULL ? g_sn : g_dumb));
	api->cleanup(api);
	return 0;
}


static void tests_sources_have_opts(void)
{
	struct lis_item *item = NULL;
	struct lis_item **children = NULL;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	enum lis_error err;

	LIS_ASSERT_EQUAL(tests_opts_init(), 0);

	err = lis_api_normalizer_all_opts_on_all_sources(g_sn, &g_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opts->get_device(g_opts, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opts[0]->name, OPT_NAME_SOURCE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opts[1]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[2], NULL);

	// should have been replicated on the children
	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	opts = NULL;
	err = children[0]->get_options(children[0], &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opts[0]->name, OPT_NAME_SOURCE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opts[1]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opts[2], NULL);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_FLATBED), 0);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_opts_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("normalizer_all_opts_on_all_souces", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_sources_have_opts()", tests_sources_have_opts) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
