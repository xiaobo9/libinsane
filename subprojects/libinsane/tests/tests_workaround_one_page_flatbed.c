#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_sn = NULL;
static struct lis_api *g_st = NULL;
static struct lis_api *g_one = NULL;


static int tests_one_init(void)
{
	enum lis_error err;
	static const union lis_value opt_source_constraint[] = {
		{ .string = OPT_VALUE_SOURCE_FLATBED, },
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
		.string = OPT_VALUE_SOURCE_FLATBED
	};

	g_one = NULL;
	g_st = NULL;
	g_sn = NULL;
	g_dumb = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_source_template, &opt_source_default);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}
	err = lis_api_normalizer_source_types(g_sn, &g_st);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_one_clean(void)
{
	struct lis_api *api;

	api = g_dumb;
	if (g_sn != NULL) {
		api = g_sn;
	}
	if (g_st != NULL) {
		api = g_st;
	}
	if (g_one != NULL) {
		api = g_one;
	}

	if (api != NULL) {
		api->cleanup(api);
	}
	return 0;
}


static void tests_one(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_RAW_RGB_24,
		.width = 4,
		.height = 2,
		.image_size = 4 * 2 * 3,
	};
	static const uint8_t body[] = {
		0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0xFF,
		0x00, 0xFF, 0x00,
		0xFF, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0xFF, 0xFF,
		0xFF, 0xFF, 0x00,
		0xFF, 0x00, 0xFF,
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
		{ .content = NULL, .nb_bytes = 0, }, // end of page
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_item **children;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize;
	uint8_t buffer[2 * 4 * 3];

	LIS_ASSERT_EQUAL(tests_one_init(), 0);
	lis_dumb_set_scan_parameters(g_dumb, &base_scan_params);
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	err = lis_api_workaround_one_page_flatbed(g_st, &g_one);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	item = NULL;
	err = g_one->get_device(g_one, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_NOT_EQUAL(children[1], NULL);
	LIS_ASSERT_NOT_EQUAL(children[2], NULL);
	LIS_ASSERT_EQUAL(children[3], NULL);

	err = children[0]->scan_start(children[0], &session);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = session->get_scan_parameters(session, &params);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(params.format, LIS_IMG_FORMAT_RAW_RGB_24);
	LIS_ASSERT_EQUAL(params.width, 4);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 4 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = sizeof(buffer);
	err = session->scan_read(session, buffer, &bufsize);;
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 4 * 2 * 3);
	LIS_ASSERT_EQUAL(buffer[0], 0xFF);
	LIS_ASSERT_EQUAL(buffer[1], 0xFF);
	LIS_ASSERT_EQUAL(buffer[2], 0xFF);
	LIS_ASSERT_EQUAL(buffer[3], 0x00);
	LIS_ASSERT_EQUAL(buffer[4], 0x00);
	LIS_ASSERT_EQUAL(buffer[5], 0xFF);
	LIS_ASSERT_EQUAL(buffer[6], 0x00);
	LIS_ASSERT_EQUAL(buffer[7], 0xFF);
	LIS_ASSERT_EQUAL(buffer[8], 0x00);

	LIS_ASSERT_TRUE(session->end_of_page(session));
	LIS_ASSERT_TRUE(session->end_of_feed(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_one_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Workaround_one_page_flatbed", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_one_page_flatbed()", tests_one) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
