#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#define LIS_UNIT_TESTS

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_lamp = NULL;


static int tests_lamp_init(void)
{
	enum lis_error err;
	static const uint8_t line_a[] = { 0x00, 0xAA, };
	static const uint8_t line_b[] = { 0x55, };
	static const uint8_t line_c[] = { 0xFF, };
	static const struct lis_dumb_read reads[] = {
		{ .content = line_a, .nb_bytes = LIS_COUNT_OF(line_a) },
		{ .content = line_b, .nb_bytes = LIS_COUNT_OF(line_b) },
		{ .content = line_c, .nb_bytes = LIS_COUNT_OF(line_c) },
	};
	static const union lis_value opt_lamp_constraint[] = {
		{ .boolean = 0, },
		{ .boolean = 1, },
	};
	static const struct lis_option_descriptor opt_lamp_template = {
		.name = OPT_NAME_LAMP_SWITCH,
		.title = "lamp-switch title",
		.desc = "lamp-switch desc",
		.capabilities = LIS_CAP_SW_SELECT,
		.value = {
			.type = LIS_TYPE_BOOL,
			.unit = LIS_UNIT_NONE,
		},
		.constraint = {
			.type = LIS_CONSTRAINT_LIST,
			.possible.list = {
				.nb_values = LIS_COUNT_OF(opt_lamp_constraint),
				.values = (union lis_value*)&opt_lamp_constraint,
			},
		},
	};
	static const union lis_value opt_lamp_default = {
		.boolean = 0,
	};

	g_dumb = NULL;
	g_lamp = NULL;

	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_add_option(g_dumb, &opt_lamp_template, &opt_lamp_default, 0);

	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	err = lis_api_workaround_lamp(g_dumb, &g_lamp);
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	return 0;
}


static int tests_lamp_clean(void)
{
	struct lis_api *api = (g_lamp != NULL ? g_lamp : g_dumb);
	api->cleanup(api);
	return 0;
}


static void check_lamp_switch(struct lis_item *item, int expected_lamp_switch, int line)
{
	struct lis_option_descriptor **opts = NULL;
	union lis_value value;
	enum lis_error err;

	err = item->get_options(item, &opts);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	LIS_ASSERT_EQUAL(strcmp(opts[0]->name, OPT_NAME_LAMP_SWITCH), 0);
	LIS_ASSERT_EQUAL(opts[1], NULL);

	LIS_ASSERT_EQUAL(opts[0]->value.type, LIS_TYPE_BOOL);

	err = opts[0]->fn.get_value(opts[0], &value);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	if (value.boolean != expected_lamp_switch) {
		fprintf(
			stdout, "L%d: Lamp switch: %d != %d\n",
			line, value.boolean, expected_lamp_switch
		);
	}
	LIS_ASSERT_EQUAL(value.boolean, expected_lamp_switch);
}


static void tests_lamp_on_off(void)
{
	struct lis_item *item;
	struct lis_scan_session *session;
	size_t bufsize;
	enum lis_error err;
	uint8_t buffer[64];

	LIS_ASSERT_EQUAL(tests_lamp_init(), 0);

	item = NULL;
	err = g_lamp->get_device(g_lamp, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	check_lamp_switch(item, 0, __LINE__);

	err = item->scan_start(item, &session);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	check_lamp_switch(item, 1, __LINE__);

	while(!session->end_of_feed(session)) {
		while(!session->end_of_page(session)) {
			bufsize = sizeof(buffer);
			err = session->scan_read(session, buffer, &bufsize);
			LIS_ASSERT_EQUAL(err, LIS_OK);
		}
	}

	check_lamp_switch(item, 0, __LINE__);

	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_lamp_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Workaround_lamp", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_lamp_on_off()", tests_lamp_on_off) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
