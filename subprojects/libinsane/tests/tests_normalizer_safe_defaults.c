#include <stdio.h>
#include <stdint.h>
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
static struct lis_api *g_opt = NULL;


static int tests_opt_init(void)
{
	static const union lis_value opt_mode_constraint[] = {
		{ .string = OPT_VALUE_MODE_BW, },
		{ .string = OPT_VALUE_MODE_COLOR, },
	};
	static const struct lis_option_descriptor opt_mode = {
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
	static const struct lis_option_descriptor opt_tlx = {
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
	static const union lis_value opt_resolution_constraint[] = {
		{ .integer = 50, },
		{ .integer = 75, },
		{ .integer = 150, },
		{ .integer = 350, },
		{ .integer = 500, },
		{ .integer = 600, },
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
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.nb_values = LIS_COUNT_OF(opt_resolution_constraint),
				.values = (union lis_value*)&opt_resolution_constraint,
			},
		},
	};
	static const union lis_value opt_resolution_default = {
		.integer = 50,
	};
	// we won't actually scan anything, but we want to start a scan session
	// so we need to feed the dumb implementation parameters & content.
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d,
	};
	static const uint8_t header_b[] = {
		0x00, 0x00,
	};
	static const uint8_t body[] = {
		0xFF, 0xFF, 0xFF,
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
	};
	enum lis_error err;

	g_opt = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(
		g_dumb, &opt_mode, &opt_mode_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_tlx, &opt_tlx_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_resolution, &opt_resolution_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_set_scan_parameters(g_dumb, &base_scan_params);
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

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
	LIS_ASSERT_EQUAL(strcasecmp(opt_opts[2]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(opt_opts[3], NULL);

	err = opt_opts[0]->fn.get_value(opt_opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(value.string, OPT_VALUE_MODE_COLOR), 0);

	err = opt_opts[1]->fn.get_value(opt_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 100);

	err = opt_opts[2]->fn.get_value(opt_opts[2], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 350);

	opt_item->close(opt_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


static void tests_opt_defaults_before_scan(void)
{
	enum lis_error err;
	struct lis_item *defaults_item = NULL;
	struct lis_item *dumb_item = NULL;
	struct lis_option_descriptor **defaults_opts = NULL;
	struct lis_option_descriptor **dumb_opts = NULL;
	struct lis_scan_session *session;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_opt_init(), 0);

	err = lis_api_normalizer_safe_defaults(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(
		g_opt, LIS_DUMB_DEV_ID_FIRST, &defaults_item
	);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_dumb->get_device(g_dumb, LIS_DUMB_DEV_ID_FIRST, &dumb_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = defaults_item->get_options(defaults_item, &defaults_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[1]->name, OPT_NAME_TL_X), 0);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[2]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(defaults_opts[3], NULL);

	// change the value without going through the safe_defaults
	// normalizer --> it should reset the value right before scanning
	err = dumb_item->get_options(dumb_item, &dumb_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	value.integer = 200;
	err = dumb_opts[1]->fn.set_value(dumb_opts[1], value, &set_flags);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = defaults_opts[1]->fn.get_value(defaults_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);

	session = NULL;
	err = defaults_item->scan_start(defaults_item, &session);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(session, NULL);

	err = dumb_opts[1]->fn.get_value(dumb_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 100); // changed back

	session->cancel(session);

	defaults_item->close(defaults_item);
	LIS_ASSERT_EQUAL(tests_opt_clean(), 0);
}


static void tests_opt_defaults_before_scan2(void)
{
	enum lis_error err;
	struct lis_item *defaults_item = NULL;
	struct lis_item *dumb_item = NULL;
	struct lis_option_descriptor **defaults_opts = NULL;
	struct lis_option_descriptor **dumb_opts = NULL;
	struct lis_scan_session *session;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_opt_init(), 0);

	err = lis_api_normalizer_safe_defaults(g_dumb, &g_opt);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_opt->get_device(
		g_opt, LIS_DUMB_DEV_ID_FIRST, &defaults_item
	);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_dumb->get_device(g_dumb, LIS_DUMB_DEV_ID_FIRST, &dumb_item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = defaults_item->get_options(defaults_item, &defaults_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[0]->name, OPT_NAME_MODE), 0);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[1]->name, OPT_NAME_TL_X), 0);
	LIS_ASSERT_EQUAL(strcasecmp(defaults_opts[2]->name, OPT_NAME_RESOLUTION), 0);
	LIS_ASSERT_EQUAL(defaults_opts[3], NULL);

	err = dumb_item->get_options(dumb_item, &dumb_opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	// change the value going through the safe_detaults
	// normalizer (as the application would do)
	// --> it shouldn't reset the value
	value.integer = 200;
	err = defaults_opts[1]->fn.set_value(
		defaults_opts[1], value, &set_flags
	);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = defaults_opts[1]->fn.get_value(defaults_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200);

	session = NULL;
	err = defaults_item->scan_start(defaults_item, &session);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(session, NULL);

	err = dumb_opts[1]->fn.get_value(dumb_opts[1], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(value.integer, 200); // unchanged

	session->cancel(session);

	defaults_item->close(defaults_item);
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

	if (CU_add_test(suite, "opt_defaults", tests_opt_defaults) == NULL
			|| CU_add_test(suite, "opt_defaults_before_scan()",
				tests_opt_defaults_before_scan) == NULL
			|| CU_add_test(suite, "opt_defaults_before_scan2()",
				tests_opt_defaults_before_scan2) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
