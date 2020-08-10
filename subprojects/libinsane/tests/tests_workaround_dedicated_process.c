#include <stdint.h>
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
static struct lis_api *g_sn = NULL;
static struct lis_api *g_opts = NULL;
static struct lis_api *g_process = NULL;


static int tests_process_init(void)
{
	enum lis_error err;
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
	static const struct lis_option_descriptor opt_xres = {
		.name = "xres",
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
	static const union lis_value opt_xres_default = {
		.integer = 120,
	};

	g_process = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(
		g_dumb, &opt_xres, &opt_xres_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_source_template, &opt_source_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}
	err = lis_api_normalizer_all_opts_on_all_sources(g_sn, &g_opts);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_process_clean(void)
{
	struct lis_api *api = (
		g_process != NULL ? g_process : (
			g_opts != NULL ? g_opts : (
				g_sn != NULL ? g_sn : (
					g_dumb
				)
			)
		)
	);
	api->cleanup(api);
	return 0;
}


static void tests_dedicated_process_opts(void)
{
	enum lis_error err;
	struct lis_device_descriptor **descs;
	struct lis_item **children;
	struct lis_item *item;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_process_init(), 0);

	err = lis_api_workaround_dedicated_process(g_opts, &g_process);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_process->list_devices(g_process, LIS_DEVICE_LOCATIONS_LOCAL_ONLY, &descs);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(descs[0], NULL);
	LIS_ASSERT_NOT_EQUAL(descs[1], NULL);
	LIS_ASSERT_EQUAL(descs[2], NULL);

	item = NULL;
	err = g_process->get_device(g_process, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_NOT_EQUAL(children[1], NULL);
	LIS_ASSERT_EQUAL(children[2], NULL);

	lis_dumb_reset_counters(g_dumb);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);

	err = children[0]->get_options(children[0], &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_NOT_EQUAL(opts[0], NULL);
	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, "xres"), 0);
	LIS_ASSERT_NOT_EQUAL(opts[1], NULL);
	LIS_ASSERT_EQUAL(strcmp(opts[1]->name, OPT_NAME_SOURCE), 0);
	LIS_ASSERT_EQUAL(opts[2], NULL);

	// TODO(Jflesch): Since the dumb implementation is running in another
	// process, those counters should remain to 0 in this process.
	/*
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	*/

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 120);

	// TODO(Jflesch): Since the dumb implementation is running in another
	// process, those counters should remain to 0 in this process.
	/*
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	*/

	value.integer = 200;
	err = opts[0]->fn.set_value(opts[0], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	// TODO(Jflesch): Since the dumb implementation is running in another
	// process, those counters should remain to 0 in this process.
	/*
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	*/

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);

	// TODO(Jflesch): Since the dumb implementation is running in another
	// process, those counters should remain to 0 in this process.
	/*
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	*/

	item->close(item);

	LIS_ASSERT_EQUAL(tests_process_clean(), 0);
}


static void tests_dedicated_process_scan(void)
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
	};

	enum lis_error err;
	struct lis_device_descriptor **descs;
	struct lis_item **children;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize;
	uint8_t buffer[2 * 4 * 3];

	LIS_ASSERT_EQUAL(tests_process_init(), 0);
	lis_dumb_set_scan_parameters(g_dumb, &base_scan_params);
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	err = lis_api_workaround_dedicated_process(g_opts, &g_process);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_process->list_devices(g_process, LIS_DEVICE_LOCATIONS_LOCAL_ONLY, &descs);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(descs[0], NULL);
	LIS_ASSERT_NOT_EQUAL(descs[1], NULL);
	LIS_ASSERT_EQUAL(descs[2], NULL);

	item = NULL;
	err = g_process->get_device(g_process, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_NOT_EQUAL(children[1], NULL);
	LIS_ASSERT_EQUAL(children[2], NULL);

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

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_process_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Workaround dedicated process", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_dedicated_process_scan()", tests_dedicated_process_scan) == NULL
		|| CU_add_test(suite, "tests_dedicated_process_opts()", tests_dedicated_process_opts) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
