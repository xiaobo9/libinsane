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
static struct lis_api *g_raw = NULL;


static int tests_raw_init(void)
{
	enum lis_error err;

	g_raw = NULL;
	err = lis_api_dumb(&g_dumb, "dummy0");
	if (LIS_IS_ERROR(err)) {
		return -1;
	}

	lis_dumb_set_nb_devices(g_dumb, 2);

	return 0;
}


static int tests_raw_clean(void)
{
	struct lis_api *api = (g_raw != NULL ? g_raw : g_dumb);
	api->cleanup(api);
	return 0;
}


static void tests_bmp2raw(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x4E, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x04, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x18, 0x00, // number of bits per pixels (24 here)
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x18, 0x00, 0x00, 0x00, // size of pixel data (4 * 2 * 3 bytes here)
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
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
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize;
	uint8_t buffer[2 * 4 * 3];

	LIS_ASSERT_EQUAL(tests_raw_init(), 0);
	lis_dumb_set_scan_parameters(g_dumb, &base_scan_params);
	lis_dumb_set_scan_result(g_dumb, reads, LIS_COUNT_OF(reads));

	err = lis_api_normalizer_bmp2raw(g_dumb, &g_raw);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	item = NULL;
	err = g_raw->get_device(g_raw, LIS_DUMB_DEV_ID_FIRST, &item);
	LIS_ASSERT_EQUAL(err, LIS_OK);

	err = item->scan_start(item, &session);
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

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


int register_tests(void)
{
	CU_pSuite suite = NULL;

	suite = CU_add_suite("Normalizer_bmp2raw", NULL, NULL);
	if (suite == NULL) {
		fprintf(stderr, "CU_add_suite() failed\n");
		return 0;
	}

	if (CU_add_test(suite, "tests_bmp2raw()", tests_bmp2raw) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
