#include <stdint.h>
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


static int tests_sn_init(void)
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
	static const uint8_t line_a[] = { 0x00, 0x1, 0x2, 0x3, 0x4 };
	static const uint8_t line_b[] = { 0x05, 0x6, 0x7, 0x8,};
	static const struct lis_dumb_read reads[] = {
		{ .content = line_a, .nb_bytes = LIS_COUNT_OF(line_a) },
		{ .content = line_b, .nb_bytes = LIS_COUNT_OF(line_b) },
		{ .content = line_a, .nb_bytes = LIS_COUNT_OF(line_a) },
		{ .content = NULL, .nb_bytes = 0 },
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
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	return 0;
}


static int tests_sn_clean(void)
{
	struct lis_api *api = (g_sn != NULL ? g_sn : g_dumb);
	api->cleanup(api);
	return 0;
}


static void tests_source_nodes(void)
{
	enum lis_error err;
	struct lis_device_descriptor **devs = NULL;
	struct lis_item *item;
	struct lis_item **children = NULL;
	int dev_idx;

	LIS_ASSERT_EQUAL(tests_sn_init(), 0);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_sn->list_devices(g_sn, LIS_DEVICE_LOCATIONS_LOCAL_ONLY, &devs);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(devs[0], NULL);

	for (dev_idx = 0 ; devs[dev_idx] != NULL ; dev_idx++) {
		item = NULL;
		err = g_sn->get_device(g_sn, devs[dev_idx]->dev_id, &item);
		LIS_ASSERT_EQUAL(err, LIS_OK);

		err = item->get_children(item, &children);
		LIS_ASSERT_EQUAL(err, LIS_OK);

		LIS_ASSERT_NOT_EQUAL(children[0], NULL);
		LIS_ASSERT_EQUAL(children[0]->name, OPT_VALUE_SOURCE_FLATBED);
		LIS_ASSERT_NOT_EQUAL(children[1], NULL);
		LIS_ASSERT_EQUAL(children[1]->name, OPT_VALUE_SOURCE_ADF);
		LIS_ASSERT_EQUAL(children[2], NULL);

		item->close(item);
	}

	LIS_ASSERT_EQUAL(tests_sn_clean(), 0);
}


static void tests_scan_start(void)
{
	enum lis_error err;
	struct lis_item *item = NULL;
	struct lis_item **children = NULL;
	struct lis_option_descriptor **opts = NULL;
	int source_opt_idx;
	union lis_value value;
	struct lis_scan_parameters scan_params;
	struct lis_scan_session *session;

	LIS_ASSERT_EQUAL(tests_sn_init(), 0);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_sn->get_device(g_sn, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	for (source_opt_idx = 0 ; opts[source_opt_idx] != NULL ; source_opt_idx++) {
		if (strcasecmp(opts[source_opt_idx]->name, OPT_NAME_SOURCE) == 0) {
			break;
		}
	}
	LIS_ASSERT_NOT_EQUAL(opts[source_opt_idx], NULL);

	err = opts[source_opt_idx]->fn.get_value(opts[source_opt_idx], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_FLATBED), 0);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_EQUAL(children[0]->name, OPT_VALUE_SOURCE_FLATBED);
	LIS_ASSERT_NOT_EQUAL(children[1], NULL);
	LIS_ASSERT_EQUAL(children[1]->name, OPT_VALUE_SOURCE_ADF);
	LIS_ASSERT_EQUAL(children[2], NULL);

	/* should trigger a change of source to make sure we get the correct parameters
	 */
	err = children[1]->scan_start(children[1], &session);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opts[source_opt_idx]->fn.get_value(opts[source_opt_idx], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_ADF), 0);

	err = session->get_scan_parameters(session, &scan_params);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	session->cancel(session);
	item->close(item);

	LIS_ASSERT_EQUAL(tests_sn_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Normalizer_source_nodes", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_source_nodes()", tests_source_nodes) == NULL
			|| CU_add_test(suite, "tests_scan_start()", tests_scan_start) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
