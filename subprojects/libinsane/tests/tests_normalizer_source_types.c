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
static struct lis_api *g_st = NULL;


static int tests_st_init(void)
{
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED " (something)", },
		{ .string = OPT_VALUE_SOURCE_ADF, },
		{ .string = "Automatic Document Feeder(left aligned)", } // Brother MFC-7360N
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
		.string = OPT_VALUE_SOURCE_FLATBED " (something)"
	};
	enum lis_error err;

	g_sn = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(
		g_dumb, &opt_source_template, &opt_source_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_st_clean(void)
{
	struct lis_api *api = (g_st != NULL ? g_st : (g_sn != NULL ? g_sn : g_dumb));
	api->cleanup(api);
	return 0;
}


static void tests_source_types(void)
{
	enum lis_error err;
	struct lis_device_descriptor **devs = NULL;
	struct lis_item *item;
	struct lis_item **children = NULL;
	int dev_idx;

	tests_st_init();

	err = lis_api_normalizer_source_types(g_sn, &g_st);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_st->list_devices(g_st, LIS_DEVICE_LOCATIONS_LOCAL_ONLY, &devs);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(devs[0], NULL);

	for (dev_idx = 0 ; devs[dev_idx] != NULL ; dev_idx++) {
		item = NULL;
		err = g_st->get_device(g_st, devs[dev_idx]->dev_id, &item);
		LIS_ASSERT_EQUAL(err, LIS_OK);

		LIS_ASSERT_EQUAL(item->type, LIS_ITEM_DEVICE);

		err = item->get_children(item, &children);
		LIS_ASSERT_EQUAL(err, LIS_OK);

		LIS_ASSERT_EQUAL(children[0]->type, LIS_ITEM_FLATBED);
		LIS_ASSERT_EQUAL(children[1]->type, LIS_ITEM_ADF);
		LIS_ASSERT_EQUAL(children[2]->type, LIS_ITEM_ADF);
		LIS_ASSERT_EQUAL(children[3], NULL);

		item->close(item);
	}

	LIS_ASSERT_EQUAL(tests_st_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Normalizer_source_types", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_source_types()", tests_source_types) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
