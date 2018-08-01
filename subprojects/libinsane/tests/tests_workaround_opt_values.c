#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_opt = NULL;


static int tests_opt_init(void)
{
	static const union lis_value opt_mode_constraint[] = {
		{ .string = "Black & White", },
		{ .string = "Gray[Error Diffusion]", },
		{ .string = "True Gray", },
		{ .string = "24bit Color", },
		{ .string = "24bit Color[Fast]", },
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
		.string = "Black & White",
	};
	enum lis_error err;

	g_opt = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_mode_template, &opt_mode_default);

	return 0;
}


static int tests_opt_clean(void)
{
	struct lis_api *api = (g_opt != NULL ? g_opt : g_dumb);
	api->cleanup(api);
	return 0;
}


static void tests_opt_values_constraint(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL;
	struct lis_option_descriptor **opts = NULL;

	tests_opt_init();

	err = lis_api_workaround_opt_values(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opt_item->get_options(opt_item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcasecmp(opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->constraint.possible.list.nb_values, 5);
	LIS_ASSERT_EQUAL(
		strcasecmp(opts[0]->constraint.possible.list.values[0].string, OPT_VALUE_MODE_BW),
		0
	);
	LIS_ASSERT_EQUAL(
		strcasecmp(opts[0]->constraint.possible.list.values[1].string, "Gray[Error Diffusion]"),
		0
	);
	LIS_ASSERT_EQUAL(
		strcasecmp(opts[0]->constraint.possible.list.values[2].string, OPT_VALUE_MODE_GRAYSCALE),
		0
	);
	LIS_ASSERT_EQUAL(
		strcasecmp(opts[0]->constraint.possible.list.values[3].string, OPT_VALUE_MODE_COLOR),
		0
	);
	LIS_ASSERT_EQUAL(
		strcasecmp(opts[0]->constraint.possible.list.values[4].string, "24bit Color[Fast]"),
		0
	);

	opt_item->close(opt_item);

	tests_opt_clean();
}


static void tests_opt_values_getset(void)
{
	enum lis_error err;
	struct lis_item *opt_item = NULL, *dumb_item = NULL;
	struct lis_option_descriptor **opt_opts = NULL, **dumb_opts = NULL;
	union lis_value opt_value;
	int set_flags;

	tests_opt_init();

	err = lis_api_workaround_opt_values(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(g_opt, LIS_DUMB_DEV_ID_FIRST, &opt_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = opt_item->get_options(opt_item, &opt_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(opt_opts[1], NULL);

	err = g_dumb->get_device(g_dumb, LIS_DUMB_DEV_ID_FIRST, &dumb_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	err = dumb_item->get_options(dumb_item, &dumb_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(dumb_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(dumb_opts[1], NULL);

	/* default value */
	err = opt_opts[0]->fn.get_value(opt_opts[0], &opt_value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_value.string, OPT_VALUE_MODE_BW), 0);

	/* change the value */
	opt_value.string = OPT_VALUE_MODE_COLOR;
	err = opt_opts[0]->fn.set_value(opt_opts[0], opt_value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opt_opts[0]->fn.get_value(opt_opts[0], &opt_value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_value.string, OPT_VALUE_MODE_COLOR), 0);

	/* check the value at the lowest level */
	err = dumb_opts[0]->fn.get_value(dumb_opts[0], &opt_value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(opt_value.string, "24bit Color"), 0);

	opt_item->close(opt_item);
	tests_opt_clean();
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("opt_values", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "test_opt_values_constraint()", tests_opt_values_constraint) == NULL
			|| CU_add_test(suite, "tests_opt_values_getset()", tests_opt_values_getset) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
