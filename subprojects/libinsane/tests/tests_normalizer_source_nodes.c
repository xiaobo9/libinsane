#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#include <libinsane/capi.h>
#include <libinsane/dumb.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_sn = NULL;


static int tests_sn_init(void)
{
	enum lis_error err;

	g_sn = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
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
	const char *sources[] = {
		"flatbed",
		"adf",
		NULL,
	};
	struct lis_device_descriptor **devs = NULL;
	struct lis_item *item;
	struct lis_item **children = NULL;
	int dev_idx;

	tests_sn_init();

	lis_dumb_set_opt_source_constraint(g_dumb, sources);

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
		LIS_ASSERT_EQUAL(children[0]->name, "flatbed");
		LIS_ASSERT_NOT_EQUAL(children[1], NULL);
		LIS_ASSERT_EQUAL(children[1]->name, "adf");
		LIS_ASSERT_EQUAL(children[2], NULL);

		item->close(item);
	}

	tests_sn_clean();
}


static void tests_get_scan_parameters(void)
{
	enum lis_error err;
	const char *sources[] = {
		"flatbed",
		"adf",
		NULL,
	};
	struct lis_device_descriptor **devs = NULL;
	struct lis_item *item = NULL;
	struct lis_item **children = NULL;
	struct lis_option_descriptor **opts = NULL;
	int source_opt_idx;
	union lis_value value;
	struct lis_scan_parameters scan_params;

	tests_sn_init();

	lis_dumb_set_opt_source_constraint(g_dumb, sources);

	err = lis_api_normalizer_source_nodes(g_dumb, &g_sn);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = g_sn->list_devices(g_sn, LIS_DEVICE_LOCATIONS_LOCAL_ONLY, &devs);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_NOT_EQUAL(devs[0], NULL);

	err = g_sn->get_device(g_sn, devs[0]->dev_id, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	for (source_opt_idx = 0 ; opts[source_opt_idx] != NULL ; source_opt_idx++) {
		if (strcasecmp(opts[source_opt_idx]->name, "source") == 0) {
			break;
		}
	}
	LIS_ASSERT_NOT_EQUAL(opts[source_opt_idx], NULL);

	err = opts[source_opt_idx]->fn.get_value(opts[source_opt_idx], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, "flatbed"), 0);

	err = item->get_children(item, &children);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_NOT_EQUAL(children[0], NULL);
	LIS_ASSERT_EQUAL(children[0]->name, "flatbed");
	LIS_ASSERT_NOT_EQUAL(children[1], NULL);
	LIS_ASSERT_EQUAL(children[1]->name, "adf");
	LIS_ASSERT_EQUAL(children[2], NULL);

	/* should trigger a change of source to make sure we get the correct parameters
	 */
	err = children[1]->get_scan_parameters(children[1], &scan_params);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = opts[source_opt_idx]->fn.get_value(opts[source_opt_idx], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(strcmp(value.string, "adf"), 0);

	item->close(item);

	tests_sn_clean();
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
			|| CU_add_test(suite, "tests_get_scan_parameters()", tests_get_scan_parameters) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
