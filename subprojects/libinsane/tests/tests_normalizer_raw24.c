#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

#define LIS_UNIT_TESTS

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/dumb.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "main.h"
#include "util.h"

#include "../src/normalizers/raw24.h"


static struct lis_api *g_dumb = NULL;
static struct lis_api *g_raw = NULL;


static int tests_raw_init(void)
{
	static const uint8_t line_a[] = { 0x00, 0xAA, };
	static const uint8_t line_b[] = { 0x55, };
	static const uint8_t line_c[] = { 0xFF, };
	static const struct lis_dumb_read reads[] = {
		{ .content = line_a, .nb_bytes = LIS_COUNT_OF(line_a) },
		{ .content = line_b, .nb_bytes = LIS_COUNT_OF(line_b) },
		{ .content = line_c, .nb_bytes = LIS_COUNT_OF(line_c) },
	};
	enum lis_error err;

	g_raw = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	return 0;
}


static int tests_raw_clean(void)
{
	struct lis_api *api = (g_raw != NULL ? g_raw : g_dumb);
	api->cleanup(api);
	return 0;
}


static void tests_unpack8(void)
{
	uint8_t buffer[32] = {
		0xAB, 0xCD,
	};
	size_t bufsize = 2;

	unpack_8_to_24(buffer, &bufsize);

	LIS_ASSERT_EQUAL(bufsize, 6);
	LIS_ASSERT_EQUAL(buffer[0], 0xAB);
	LIS_ASSERT_EQUAL(buffer[1], 0xAB);
	LIS_ASSERT_EQUAL(buffer[2], 0xAB);
	LIS_ASSERT_EQUAL(buffer[3], 0xCD);
	LIS_ASSERT_EQUAL(buffer[4], 0xCD);
	LIS_ASSERT_EQUAL(buffer[5], 0xCD);
}


static void tests_unpack1(void)
{
	uint8_t buffer[48] = {
		0xAB, // 0b10101011
		0xCD, // 0b11001101
	};
	size_t bufsize = 2;

	unpack_1_to_24(buffer, &bufsize);

	LIS_ASSERT_EQUAL(bufsize, 48);

	// 0xAB
	LIS_ASSERT_EQUAL(buffer[0], 0x00);
	LIS_ASSERT_EQUAL(buffer[1], 0x00);
	LIS_ASSERT_EQUAL(buffer[2], 0x00);
	LIS_ASSERT_EQUAL(buffer[3], 0xFF);
	LIS_ASSERT_EQUAL(buffer[4], 0xFF);
	LIS_ASSERT_EQUAL(buffer[5], 0xFF);
	LIS_ASSERT_EQUAL(buffer[6], 0x00);
	LIS_ASSERT_EQUAL(buffer[7], 0x00);
	LIS_ASSERT_EQUAL(buffer[8], 0x00);
	LIS_ASSERT_EQUAL(buffer[9], 0xFF);
	LIS_ASSERT_EQUAL(buffer[10], 0xFF);
	LIS_ASSERT_EQUAL(buffer[11], 0xFF);
	LIS_ASSERT_EQUAL(buffer[12], 0x00);
	LIS_ASSERT_EQUAL(buffer[13], 0x00);
	LIS_ASSERT_EQUAL(buffer[14], 0x00);
	LIS_ASSERT_EQUAL(buffer[15], 0xFF);
	LIS_ASSERT_EQUAL(buffer[16], 0xFF);
	LIS_ASSERT_EQUAL(buffer[17], 0xFF);
	LIS_ASSERT_EQUAL(buffer[18], 0x00);
	LIS_ASSERT_EQUAL(buffer[19], 0x00);
	LIS_ASSERT_EQUAL(buffer[20], 0x00);
	LIS_ASSERT_EQUAL(buffer[21], 0x00);
	LIS_ASSERT_EQUAL(buffer[22], 0x00);
	LIS_ASSERT_EQUAL(buffer[23], 0x00);

	// 0xCD
	LIS_ASSERT_EQUAL(buffer[24], 0x00);
	LIS_ASSERT_EQUAL(buffer[25], 0x00);
	LIS_ASSERT_EQUAL(buffer[26], 0x00);
	LIS_ASSERT_EQUAL(buffer[27], 0x00);
	LIS_ASSERT_EQUAL(buffer[28], 0x00);
	LIS_ASSERT_EQUAL(buffer[29], 0x00);
	LIS_ASSERT_EQUAL(buffer[30], 0xFF);
	LIS_ASSERT_EQUAL(buffer[31], 0xFF);
	LIS_ASSERT_EQUAL(buffer[32], 0xFF);
	LIS_ASSERT_EQUAL(buffer[33], 0xFF);
	LIS_ASSERT_EQUAL(buffer[34], 0xFF);
	LIS_ASSERT_EQUAL(buffer[35], 0xFF);
	LIS_ASSERT_EQUAL(buffer[36], 0x00);
	LIS_ASSERT_EQUAL(buffer[37], 0x00);
	LIS_ASSERT_EQUAL(buffer[38], 0x00);
	LIS_ASSERT_EQUAL(buffer[39], 0x00);
	LIS_ASSERT_EQUAL(buffer[40], 0x00);
	LIS_ASSERT_EQUAL(buffer[41], 0x00);
	LIS_ASSERT_EQUAL(buffer[42], 0xFF);
	LIS_ASSERT_EQUAL(buffer[43], 0xFF);
	LIS_ASSERT_EQUAL(buffer[44], 0xFF);
	LIS_ASSERT_EQUAL(buffer[45], 0x00);
	LIS_ASSERT_EQUAL(buffer[46], 0x00);
	LIS_ASSERT_EQUAL(buffer[47], 0x00);
}


static void tests_raw8(void)
{
	static const struct lis_scan_parameters params = {
		.format = LIS_IMG_FORMAT_GRAYSCALE_8,
		.width = 2,
		.height = 2,
		.image_size = 4,
	};
	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	uint8_t buffer[32];
	size_t bufsize;
	struct lis_scan_parameters out_params;

	LIS_ASSERT_EQUAL(tests_raw_init(), 0);

	lis_dumb_set_scan_parameters(g_dumb, &params);
	err = lis_api_normalizer_raw24(g_dumb, &g_raw);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	item = NULL;
	err = g_raw->get_device(g_raw, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->scan_start(item, &session);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = session->get_scan_parameters(session, &out_params);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(out_params.format, LIS_IMG_FORMAT_RAW_RGB_24);
	LIS_ASSERT_EQUAL(out_params.width, 2);
	LIS_ASSERT_EQUAL(out_params.height, 2);
	LIS_ASSERT_EQUAL(out_params.image_size, 4 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = sizeof(buffer);
	err = session->scan_read(session, buffer, &bufsize);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 6);
	LIS_ASSERT_EQUAL(buffer[0], 0x00);
	LIS_ASSERT_EQUAL(buffer[1], 0x00);
	LIS_ASSERT_EQUAL(buffer[2], 0x00);
	LIS_ASSERT_EQUAL(buffer[3], 0xAA);
	LIS_ASSERT_EQUAL(buffer[4], 0xAA);
	LIS_ASSERT_EQUAL(buffer[5], 0xAA);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = 2;
	err = session->scan_read(session, buffer, &bufsize);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 0);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = 4;
	err = session->scan_read(session, buffer, &bufsize);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 3);
	LIS_ASSERT_EQUAL(buffer[0], 0x55);
	LIS_ASSERT_EQUAL(buffer[1], 0x55);
	LIS_ASSERT_EQUAL(buffer[2], 0x55);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = sizeof(buffer);
	err = session->scan_read(session, buffer, &bufsize);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 3);
	LIS_ASSERT_EQUAL(buffer[0], 0xFF);
	LIS_ASSERT_EQUAL(buffer[1], 0xFF);
	LIS_ASSERT_EQUAL(buffer[2], 0xFF);

	LIS_ASSERT_TRUE(session->end_of_page(session));
	LIS_ASSERT_TRUE(session->end_of_feed(session));
	session->cancel(session);

	item->close(item);
	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_raw1(void)
{
	static const struct lis_scan_parameters params = {
		.format = LIS_IMG_FORMAT_BW_1,
		.width = 16,
		.height = 16,
		.image_size = (16 * 16) / 8,
	};
	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	uint8_t buffer[64];
	size_t bufsize;
	struct lis_scan_parameters out_params;

	LIS_ASSERT_EQUAL(tests_raw_init(), 0);

	lis_dumb_set_scan_parameters(g_dumb, &params);
	err = lis_api_normalizer_raw24(g_dumb, &g_raw);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	item = NULL;
	err = g_raw->get_device(g_raw, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->scan_start(item, &session);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = session->get_scan_parameters(session, &out_params);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(out_params.format, LIS_IMG_FORMAT_RAW_RGB_24);
	LIS_ASSERT_EQUAL(out_params.width, 16);
	LIS_ASSERT_EQUAL(out_params.height, 16);
	LIS_ASSERT_EQUAL(out_params.image_size, 16 * 16 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));
	bufsize = sizeof(buffer);
	err = session->scan_read(session, buffer, &bufsize);
	LIS_ASSERT_EQUAL(err, LIS_OK);
	LIS_ASSERT_EQUAL(bufsize, 2 * 8 * 3);
	LIS_ASSERT_EQUAL(buffer[0], 0xFF);
	LIS_ASSERT_EQUAL(buffer[1], 0xFF);
	LIS_ASSERT_EQUAL(buffer[2], 0xFF);
	LIS_ASSERT_EQUAL(buffer[3], 0xFF);
	LIS_ASSERT_EQUAL(buffer[4], 0xFF);
	LIS_ASSERT_EQUAL(buffer[5], 0xFF);
	LIS_ASSERT_EQUAL(buffer[6], 0xFF);
	LIS_ASSERT_EQUAL(buffer[7], 0xFF);
	LIS_ASSERT_EQUAL(buffer[8], 0xFF);
	LIS_ASSERT_EQUAL(buffer[9], 0xFF);
	LIS_ASSERT_EQUAL(buffer[10], 0xFF);
	LIS_ASSERT_EQUAL(buffer[11], 0xFF);
	LIS_ASSERT_EQUAL(buffer[12], 0xFF);
	LIS_ASSERT_EQUAL(buffer[13], 0xFF);
	LIS_ASSERT_EQUAL(buffer[14], 0xFF);
	LIS_ASSERT_EQUAL(buffer[15], 0xFF);
	LIS_ASSERT_EQUAL(buffer[16], 0xFF);
	LIS_ASSERT_EQUAL(buffer[17], 0xFF);
	LIS_ASSERT_EQUAL(buffer[18], 0xFF);
	LIS_ASSERT_EQUAL(buffer[19], 0xFF);
	LIS_ASSERT_EQUAL(buffer[20], 0xFF);
	LIS_ASSERT_EQUAL(buffer[21], 0xFF);
	LIS_ASSERT_EQUAL(buffer[22], 0xFF);
	LIS_ASSERT_EQUAL(buffer[23], 0xFF);

	LIS_ASSERT_EQUAL(buffer[24], 0x00);
	LIS_ASSERT_EQUAL(buffer[25], 0x00);
	LIS_ASSERT_EQUAL(buffer[26], 0x00);
	LIS_ASSERT_EQUAL(buffer[27], 0xFF);
	LIS_ASSERT_EQUAL(buffer[28], 0xFF);
	LIS_ASSERT_EQUAL(buffer[29], 0xFF);
	LIS_ASSERT_EQUAL(buffer[30], 0x00);
	LIS_ASSERT_EQUAL(buffer[31], 0x00);
	LIS_ASSERT_EQUAL(buffer[32], 0x00);
	LIS_ASSERT_EQUAL(buffer[33], 0xFF);
	LIS_ASSERT_EQUAL(buffer[34], 0xFF);
	LIS_ASSERT_EQUAL(buffer[35], 0xFF);
	LIS_ASSERT_EQUAL(buffer[36], 0x00);
	LIS_ASSERT_EQUAL(buffer[37], 0x00);
	LIS_ASSERT_EQUAL(buffer[38], 0x00);
	LIS_ASSERT_EQUAL(buffer[39], 0xFF);
	LIS_ASSERT_EQUAL(buffer[40], 0xFF);
	LIS_ASSERT_EQUAL(buffer[41], 0xFF);
	LIS_ASSERT_EQUAL(buffer[42], 0x00);
	LIS_ASSERT_EQUAL(buffer[43], 0x00);
	LIS_ASSERT_EQUAL(buffer[44], 0x00);
	LIS_ASSERT_EQUAL(buffer[45], 0xFF);
	LIS_ASSERT_EQUAL(buffer[46], 0xFF);
	LIS_ASSERT_EQUAL(buffer[47], 0xFF);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	LIS_ASSERT_FALSE(session->end_of_page(session));

	session->cancel(session);

	item->close(item);
	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Normalizer_raw24", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_unpack8()", tests_unpack8) == NULL
			|| CU_add_test(suite, "tests_unpack1()", tests_unpack1) == NULL
			|| CU_add_test(suite, "tests_raw8()", tests_raw8) == NULL
			|| CU_add_test(suite, "tests_raw1()", tests_raw1)
				== NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
