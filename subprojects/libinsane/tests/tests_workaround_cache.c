#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>
#include <libinsane/workarounds.h>

#include "main.h"
#include "util.h"


static struct lis_api *g_dumb = NULL;
// beware, g_cache is wrapped by g_sn / g_opts in those tests
static struct lis_api *g_cache = NULL;
static struct lis_api *g_sn = NULL;
static struct lis_api *g_opts = NULL;


static int tests_cache_init(void)
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
	g_cache = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices_with_type(g_dumb, 2, LIS_ITEM_ADF);
	lis_dumb_add_option(
		g_dumb, &opt_source_template, &opt_source_default,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	lis_dumb_add_option(
		g_dumb, &opt_resolution, &opt_resolution_default,
		LIS_SET_FLAG_MUST_RELOAD_OPTIONS
	);

	err = lis_api_workaround_cache(g_dumb, &g_cache);
	if (LIS_IS_ERROR(err)) {
		g_dumb->cleanup(g_dumb);
		return -1;
	}

	err = lis_api_normalizer_source_nodes(g_cache, &g_sn);
	if (LIS_IS_ERROR(err)) {
		g_cache->cleanup(g_cache);
		return -1;
	}

	err = lis_api_normalizer_all_opts_on_all_sources(g_sn, &g_opts);
	if (LIS_IS_ERROR(err)) {
		g_sn->cleanup(g_sn);
		return -1;
	}

	return 0;
}


static int tests_cache_cleanup(void)
{
	g_opts->cleanup(g_opts);
	return 0;
}


static void test_cache_list_options(void)
{
	enum lis_error err;
	struct lis_item *device = NULL;
	struct lis_item **sources = NULL;
	struct lis_option_descriptor **opts = NULL;

	LIS_ASSERT_EQUAL(tests_cache_init(), 0);

	err = g_opts->get_device(
		g_opts, LIS_DUMB_DEV_ID_FIRST, &device
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device, NULL);

	LIS_ASSERT_EQUAL(device->type, LIS_ITEM_ADF);

	err = device->get_children(device, &sources);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(sources, NULL);
	LIS_ASSERT_NOT_EQUAL(sources[0], NULL);

	// source normalizer has already requested options once ...
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 1);
	err = sources[0]->get_options(sources[0], &opts);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 1);

	err = sources[0]->get_options(sources[0], &opts);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_list_options(g_dumb), 1);

	device->close(device);
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_get_value(void)
{
	enum lis_error err;
	struct lis_item *device = NULL;
	struct lis_item **sources = NULL;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;

	LIS_ASSERT_EQUAL(tests_cache_init(), 0);

	err = g_opts->get_device(
		g_opts, LIS_DUMB_DEV_ID_FIRST, &device
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device, NULL);

	err = device->get_children(device, &sources);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(sources, NULL);
	LIS_ASSERT_NOT_EQUAL(sources[0], NULL);

	err = sources[0]->get_options(sources[0], &opts);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_NOT_EQUAL(opts[0], NULL);

	// source node normalizer tried to set the source --> cache
	// workaround made a call to get_value() first and saw the
	// correct value was already set
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);
	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_FLATBED), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_FLATBED), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);

	device->close(device);
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_set_value(void)
{
	enum lis_error err;
	struct lis_item *device = NULL;
	struct lis_item **sources = NULL;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_cache_init(), 0);

	err = g_opts->get_device(
		g_opts, LIS_DUMB_DEV_ID_FIRST, &device
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device, NULL);

	err = device->get_children(device, &sources);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(sources, NULL);
	LIS_ASSERT_NOT_EQUAL(sources[0], NULL);

	err = sources[0]->get_options(sources[0], &opts);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_NOT_EQUAL(opts[0], NULL);

	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	value.string = OPT_VALUE_SOURCE_ADF;
	err = opts[0]->fn.set_value(opts[0], value, &set_flags);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(
		set_flags,
		LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);
	// cache gets the current value before setting it
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 1);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(strcmp(value.string, OPT_VALUE_SOURCE_ADF), 0);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 1);

	device->close(device);
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_set_value_2(void)
{
	enum lis_error err;
	struct lis_item *device = NULL;
	struct lis_item **sources = NULL;
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	int set_flags;

	LIS_ASSERT_EQUAL(tests_cache_init(), 0);

	err = g_opts->get_device(
		g_opts, LIS_DUMB_DEV_ID_FIRST, &device
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device, NULL);

	err = device->get_children(device, &sources);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(sources, NULL);
	LIS_ASSERT_NOT_EQUAL(sources[0], NULL);

	err = sources[0]->get_options(sources[0], &opts);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(opts, NULL);
	LIS_ASSERT_NOT_EQUAL(opts[0], NULL);
	LIS_ASSERT_NOT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 1);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 0);
	value.integer = 240;
	err = opts[1]->fn.set_value(opts[1], value, &set_flags);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(
		set_flags,
		LIS_SET_FLAG_MUST_RELOAD_OPTIONS
	);
	// cache gets the current value before setting it
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 2);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 1);

	// since we got flag 'reload_options', this get_value() will go through
	err = opts[1]->fn.get_value(opts[1], &value);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_EQUAL(value.integer, 240);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_get(g_dumb), 3);
	LIS_ASSERT_EQUAL(lis_dumb_get_nb_set(g_dumb), 1);

	device->close(device);
	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


static void test_cache_double_get_device(void)
{
	enum lis_error err;
	struct lis_item *device = NULL;
	struct lis_item *device2 = NULL;

	LIS_ASSERT_EQUAL(tests_cache_init(), 0);

	err = g_cache->get_device(
		g_cache, LIS_DUMB_DEV_ID_FIRST, &device
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device, NULL);

	err = g_cache->get_device(
		g_cache, LIS_DUMB_DEV_ID_FIRST, &device2
	);
	LIS_ASSERT_TRUE(LIS_IS_OK(err));
	LIS_ASSERT_NOT_EQUAL(device2, NULL);
	LIS_ASSERT_EQUAL(device, device2);

	device->close(device);
	device2->close(device2);

	LIS_ASSERT_EQUAL(tests_cache_cleanup(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("cache", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "list_options", test_cache_list_options) == NULL
			|| CU_add_test(suite, "get_value", test_cache_get_value) == NULL
			|| CU_add_test(suite, "set_value", test_cache_set_value) == NULL
			|| CU_add_test(suite, "set_value_2", test_cache_set_value_2) == NULL
			|| CU_add_test(suite, "double_get_device", test_cache_double_get_device) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}

