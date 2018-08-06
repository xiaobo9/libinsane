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
static struct lis_api *g_opt = NULL;


static enum lis_error nogo()
{
	return LIS_ERR_IO_ERROR;
}


static int tests_opt_init(void)
{
	static const union lis_value opt_mode_constraint[] = {
		{ .string = OPT_VALUE_MODE_BW, },
		{ .string = OPT_VALUE_MODE_COLOR, },
	};
	static const struct lis_option_descriptor opt_mode_template = {
		.name = OPT_NAME_MODE,
		.title = "mode title",
		.desc = "mode desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_STRING,
			.unit = LIS_UNIT_NONE,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.nb_values = LIS_COUNT_OF(opt_mode_constraint),
				.values = (union lis_value*)&opt_mode_constraint,
			},
		},
	};
	static const union lis_value opt_mode_default = {
		.string = OPT_VALUE_MODE_BW,
	};
	static const struct lis_option_descriptor opt_tlx_template = {
		.name = OPT_NAME_TL_X,
		.title = "tl-x title",
		.desc = "tl-x desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_INTEGER,
			.unit = LIS_UNIT_PIXEL,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_RANGE,
			.possible = {
				.range = {
					.min.integer = 100,
					.max.integer = 1000,
					.interval.integer = 10,
				},
			},
		},
	};
	static const union lis_value opt_tlx_default = {
		.integer = 300,
	};
	enum lis_error err;

	g_opt = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_mode_template, &opt_mode_default);
	lis_dumb_add_option(g_dumb, &opt_tlx_template, &opt_tlx_default);

	return 0;
}


static int tests_opt_clean(void)
{
	struct lis_api *api = (g_opt != NULL ? g_opt : g_dumb);
	api->cleanup(api);
	return 0;
}

static void tests_opt_defaults(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL;
	struct lis_option_descriptor **opt_opts = NULL;
	union lis_value value;

	LIS_ASSERT_EQUAL(tests_opt_init(), 0);

	err = lis_api_normalizer_safe_defaults(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[1]->name, OPT_NAME_TL_X), 0);
	LIS_ASSERT_EQUAL(opt_opts[2], NULL);

	err = opt_opts[0]->fn.get_value(opt_opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(value.string, OPT_VALUE_MODE_COLOR), 0);

	err = opt_opts[1]->fn.get_value(opt_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 100);


	opt_item->close(opt_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


static void tests_opt_scan_area_failed(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL;
	struct lis_option_descriptor **opt_opts = NULL;
	union lis_value value;
	int opt_idx;

	LIS_ASSERT_EQUAL(tests_opt_init(), 0);

	/* change the callback for tl_x->set_value */
	err = g_dumb->get_device(g_dumb, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	for (opt_idx = 0 ; opt_opts[opt_idx] != NULL ; opt_idx++) {
		if (strcasecmp(opt_opts[opt_idx]->name, OPT_NAME_TL_X) == 0) {
			opt_opts[opt_idx]->fn.set_value = nogo;
			opt_opts[opt_idx]->capabilities = 0;
			break;
		}
	}
	LIS_ASSERT_NOT_EQUAL(opt_opts[opt_idx], NULL);

	/* test that things still work */
	err = lis_api_normalizer_safe_defaults(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[1]->name, OPT_NAME_TL_X), 0);
	LIS_ASSERT_EQUAL(opt_opts[2], NULL);

	err = opt_opts[0]->fn.get_value(opt_opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(value.string, OPT_VALUE_MODE_COLOR), 0);

	/* tl_x value should be unchanged */
	err = opt_opts[1]->fn.get_value(opt_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 300);

	opt_item->close(opt_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("opt_mode", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_opt_defaults()", tests_opt_defaults) == NULL
			|| CU_add_test(suite, "test_opt_scan_area_failed", tests_opt_scan_area_failed) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
