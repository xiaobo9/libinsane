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
static struct lis_api *g_snodes = NULL;
static struct lis_api *g_snames = NULL;

static int tests_sn_init(void)
{
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED, },
		{ .string = OPT_VALUE_SOURCE_ADF " COW", },
		{ .string = "flatbed TRUCK", },
		{ .string = "ADF camion", },
		{ .string = "Automatic document Feeder TULIPE", },
		{ .string = "0000\\Root\\Flatbed MEH", },
		{ .string = "Document Table YOP" },
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
		.string = OPT_VALUE_SOURCE_ADF
	};
	enum lis_error err;

	g_dumb = NULL;
	g_snodes = NULL;
	g_snames = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_source_template, &opt_source_default);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_snodes);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_sn_clean(void)
{
	struct lis_api *api = (g_snames != NULL ? g_snames : (g_snodes != NULL ? g_snodes : g_dumb));
	api->cleanup(api);
	return 0;
}


static void tests_source_names(void)
{
	enum lis_error err;
	struct lis_item *root;
	struct lis_item **sources;

	LIS_ASSERT_EQUAL(tests_sn_init(), 0);

	err = lis_api_normalizer_source_names(g_snodes, &g_snames);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_snames->get_device(g_snames, LIS_DUMB_DEV_ID_FIRST, &root);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = root->get_children(root, &sources);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(sources[0]->name, OPT_VALUE_SOURCE_FLATBED), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[1]->name, OPT_VALUE_SOURCE_ADF " COW"), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[2]->name, OPT_VALUE_SOURCE_FLATBED " TRUCK"), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[3]->name, OPT_VALUE_SOURCE_ADF " camion"), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[4]->name, OPT_VALUE_SOURCE_ADF " TULIPE"), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[5]->name, OPT_VALUE_SOURCE_FLATBED " meh"), 0);
	LIS_ASSERT_EQUAL(strcmp(sources[6]->name, OPT_VALUE_SOURCE_FLATBED " YOP"), 0);
	LIS_ASSERT_EQUAL(sources[7], NULL);

	root->close(root);

	LIS_ASSERT_EQUAL(tests_sn_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("normalizer_source_names", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_source_names()", tests_source_names) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
