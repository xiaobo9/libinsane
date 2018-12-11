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
static struct lis_api *g_src = NULL;


static int tests_src_init(void)
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
	enum lis_error err;

	g_src = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_mode_template, &opt_mode_default);

	return 0;
}


static int tests_src_clean(void)
{
	struct lis_api *api = (g_src != NULL ? g_src : g_dumb);
	api->cleanup(api);
	return 0;
}

static void tests_src_default_source(void)
{
	enum lis_error err;
	struct lis_item *root;
	struct lis_item **children;
	struct lis_option_descriptor **opts;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_src_init(), 0);

	err = lis_api_normalizer_min_one_source(g_dumb, &g_src);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_src->get_device(g_src, LIS_DUMB_DEV_ID_FIRST, &root);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = root->get_children(root, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_EQUAL(children[1], NULL);

	LIS_ASSERT_EQUAL(strcmp(children[0]->name, OPT_VALUE_SOURCE_FLATBED), 0);

	err = children[0]->get_options(children[0], &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(opts[0], NULL);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_MODE), 0);

	value.string = OPT_VALUE_MODE_COLOR;
	err = opts[0]->fn.set_value(opts[0], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	root->close(root);
	LIS_ASSERT_EQUAL(tests_src_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("min_one_source", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_src_default_source()", tests_src_default_source) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
