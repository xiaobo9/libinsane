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


static void tests_bmp2raw_24(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x48, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x18, 0x00, // number of bits per pixels (24 here)
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x20, 0x00, 0x00, 0x00, // size of pixel data (((5 * 3) + 1) * 2 bytes here)
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t body_a[] = {
		// row N-1
		0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0xFF,
		0x00, 0xFF, 0x00,
	};
	static const uint8_t body_b[] = {
		0xFF, 0x00, 0x00,
		0x01, 0x02, 0x03,
	};
	static const uint8_t body_c[] = {
		0x00, // padding
	};
	static const uint8_t body_d[] = {
		// row N-2
		0x00, 0x00, 0x00,
		0x00, 0xFF, 0xFF,
		0xFF, 0xFF, 0x00,
		0xFF, 0x00, 0xFF,
		0x04, 0x05, 0x06,
	};
	static const uint8_t body_e[] = {
		0x00, // padding
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = body_a, .nb_bytes = LIS_COUNT_OF(body_a) },
		{ .content = body_b, .nb_bytes = LIS_COUNT_OF(body_b) },
		{ .content = body_c, .nb_bytes = LIS_COUNT_OF(body_c) },
		{ .content = body_d, .nb_bytes = LIS_COUNT_OF(body_d) },
		{ .content = body_e, .nb_bytes = LIS_COUNT_OF(body_e) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row N-1 (reversed)
	LIS_ASSERT_EQUAL(buffer[0], 0x03);
	LIS_ASSERT_EQUAL(buffer[1], 0x02);
	LIS_ASSERT_EQUAL(buffer[2], 0x01);

	LIS_ASSERT_EQUAL(buffer[3], 0x00);
	LIS_ASSERT_EQUAL(buffer[4], 0x00);
	LIS_ASSERT_EQUAL(buffer[5], 0xFF);

	LIS_ASSERT_EQUAL(buffer[6], 0x00);
	LIS_ASSERT_EQUAL(buffer[7], 0xFF);
	LIS_ASSERT_EQUAL(buffer[8], 0x00);

	LIS_ASSERT_EQUAL(buffer[9], 0xFF);
	LIS_ASSERT_EQUAL(buffer[10], 0x00);
	LIS_ASSERT_EQUAL(buffer[11], 0x00);

	LIS_ASSERT_EQUAL(buffer[12], 0xFF);
	LIS_ASSERT_EQUAL(buffer[13], 0xFF);
	LIS_ASSERT_EQUAL(buffer[14], 0xFF);

	// row N-2 (reversed)
	LIS_ASSERT_EQUAL(buffer[15], 0x06);
	LIS_ASSERT_EQUAL(buffer[16], 0x05);
	LIS_ASSERT_EQUAL(buffer[17], 0x04);

	LIS_ASSERT_EQUAL(buffer[18], 0xFF);
	LIS_ASSERT_EQUAL(buffer[19], 0x00);
	LIS_ASSERT_EQUAL(buffer[20], 0xFF);

	LIS_ASSERT_EQUAL(buffer[21], 0x00);
	LIS_ASSERT_EQUAL(buffer[22], 0xFF);
	LIS_ASSERT_EQUAL(buffer[23], 0xFF);

	LIS_ASSERT_EQUAL(buffer[24], 0xFF);
	LIS_ASSERT_EQUAL(buffer[25], 0xFF);
	LIS_ASSERT_EQUAL(buffer[26], 0x00);

	LIS_ASSERT_EQUAL(buffer[27], 0x00);
	LIS_ASSERT_EQUAL(buffer[28], 0x00);
	LIS_ASSERT_EQUAL(buffer[29], 0x00);

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_bmp2raw_24_top_to_bottom(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x48, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0xFE, 0xFF, // height (1st part) (top to bottom)
	};
	static const uint8_t header_b[] = {
		0xFF, 0xFF, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x18, 0x00, // number of bits per pixels (24 here)
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x20, 0x00, 0x00, 0x00, // size of pixel data (((5 * 3) + 1) * 2 bytes here)
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t body_a[] = {
		// row N-1
		0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0xFF,
		0x00, 0xFF, 0x00,
	};
	static const uint8_t body_b[] = {
		0xFF, 0x00, 0x00,
		0x01, 0x02, 0x03,
	};
	static const uint8_t body_c[] = {
		0x00, // padding
	};
	static const uint8_t body_d[] = {
		// row N-2
		0x00, 0x00, 0x00,
		0x00, 0xFF, 0xFF,
		0xFF, 0xFF, 0x00,
		0xFF, 0x00, 0xFF,
		0x04, 0x05, 0x06,
	};
	static const uint8_t body_e[] = {
		0x00, // padding
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = body_a, .nb_bytes = LIS_COUNT_OF(body_a) },
		{ .content = body_b, .nb_bytes = LIS_COUNT_OF(body_b) },
		{ .content = body_c, .nb_bytes = LIS_COUNT_OF(body_c) },
		{ .content = body_d, .nb_bytes = LIS_COUNT_OF(body_d) },
		{ .content = body_e, .nb_bytes = LIS_COUNT_OF(body_e) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row 0 (reversed)
	LIS_ASSERT_EQUAL(buffer[12], 0x03);
	LIS_ASSERT_EQUAL(buffer[13], 0x02);
	LIS_ASSERT_EQUAL(buffer[14], 0x01);

	LIS_ASSERT_EQUAL(buffer[9], 0x00);
	LIS_ASSERT_EQUAL(buffer[10], 0x00);
	LIS_ASSERT_EQUAL(buffer[11], 0xFF);

	LIS_ASSERT_EQUAL(buffer[6], 0x00);
	LIS_ASSERT_EQUAL(buffer[7], 0xFF);
	LIS_ASSERT_EQUAL(buffer[8], 0x00);

	LIS_ASSERT_EQUAL(buffer[3], 0xFF);
	LIS_ASSERT_EQUAL(buffer[4], 0x00);
	LIS_ASSERT_EQUAL(buffer[5], 0x00);

	LIS_ASSERT_EQUAL(buffer[0], 0xFF);
	LIS_ASSERT_EQUAL(buffer[1], 0xFF);
	LIS_ASSERT_EQUAL(buffer[2], 0xFF);

	// row 1 (reversed)
	LIS_ASSERT_EQUAL(buffer[27], 0x06);
	LIS_ASSERT_EQUAL(buffer[28], 0x05);
	LIS_ASSERT_EQUAL(buffer[29], 0x04);

	LIS_ASSERT_EQUAL(buffer[24], 0xFF);
	LIS_ASSERT_EQUAL(buffer[25], 0x00);
	LIS_ASSERT_EQUAL(buffer[26], 0xFF);

	LIS_ASSERT_EQUAL(buffer[21], 0x00);
	LIS_ASSERT_EQUAL(buffer[22], 0xFF);
	LIS_ASSERT_EQUAL(buffer[23], 0xFF);

	LIS_ASSERT_EQUAL(buffer[18], 0xFF);
	LIS_ASSERT_EQUAL(buffer[19], 0xFF);
	LIS_ASSERT_EQUAL(buffer[20], 0x00);

	LIS_ASSERT_EQUAL(buffer[15], 0x00);
	LIS_ASSERT_EQUAL(buffer[16], 0x00);
	LIS_ASSERT_EQUAL(buffer[17], 0x00);

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_bmp2raw_8(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x46, 0x04, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x04, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x08, 0x00, // number of bits per pixels
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x10, 0x00, 0x00, 0x00, // size of pixel data (5px + padding) * 2px
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x01, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t palette[] = {
		// use a weird palette to make sure we didn't fall back to
		// a default one
		0x00, 0xFF, 0xFE, 0x00,
		0x01, 0x00, 0xFF, 0x00,
		0x02, 0x01, 0x00, 0x00,
		0x03, 0x02, 0x01, 0x00,
		0x04, 0x03, 0x02, 0x00,
		0x05, 0x04, 0x03, 0x00,
		0x06, 0x05, 0x04, 0x00,
		0x07, 0x06, 0x05, 0x00,
		0x08, 0x07, 0x06, 0x00,
		0x09, 0x08, 0x07, 0x00,
		0x0A, 0x09, 0x08, 0x00,
		0x0B, 0x0A, 0x09, 0x00,
		0x0C, 0x0B, 0x0A, 0x00,
		0x0D, 0x0C, 0x0B, 0x00,
		0x0E, 0x0D, 0x0C, 0x00,
		0x0F, 0x0E, 0x0D, 0x00,
		0x10, 0x0F, 0x0E, 0x00,
		0x11, 0x10, 0x0F, 0x00,
		0x12, 0x11, 0x10, 0x00,
		0x13, 0x12, 0x11, 0x00,
		0x14, 0x13, 0x12, 0x00,
		0x15, 0x14, 0x13, 0x00,
		0x16, 0x15, 0x14, 0x00,
		0x17, 0x16, 0x15, 0x00,
		0x18, 0x17, 0x16, 0x00,
		0x19, 0x18, 0x17, 0x00,
		0x1A, 0x19, 0x18, 0x00,
		0x1B, 0x1A, 0x19, 0x00,
		0x1C, 0x1B, 0x1A, 0x00,
		0x1D, 0x1C, 0x1B, 0x00,
		0x1E, 0x1D, 0x1C, 0x00,
		0x1F, 0x1E, 0x1D, 0x00,
		0x20, 0x1F, 0x1E, 0x00,
		0x21, 0x20, 0x1F, 0x00,
		0x22, 0x21, 0x20, 0x00,
		0x23, 0x22, 0x21, 0x00,
		0x24, 0x23, 0x22, 0x00,
		0x25, 0x24, 0x23, 0x00,
		0x26, 0x25, 0x24, 0x00,
		0x27, 0x26, 0x25, 0x00,
		0x28, 0x27, 0x26, 0x00,
		0x29, 0x28, 0x27, 0x00,
		0x2A, 0x29, 0x28, 0x00,
		0x2B, 0x2A, 0x29, 0x00,
		0x2C, 0x2B, 0x2A, 0x00,
		0x2D, 0x2C, 0x2B, 0x00,
		0x2E, 0x2D, 0x2C, 0x00,
		0x2F, 0x2E, 0x2D, 0x00,
		0x30, 0x2F, 0x2E, 0x00,
		0x31, 0x30, 0x2F, 0x00,
		0x32, 0x31, 0x30, 0x00,
		0x33, 0x32, 0x31, 0x00,
		0x34, 0x33, 0x32, 0x00,
		0x35, 0x34, 0x33, 0x00,
		0x36, 0x35, 0x34, 0x00,
		0x37, 0x36, 0x35, 0x00,
		0x38, 0x37, 0x36, 0x00,
		0x39, 0x38, 0x37, 0x00,
		0x3A, 0x39, 0x38, 0x00,
		0x3B, 0x3A, 0x39, 0x00,
		0x3C, 0x3B, 0x3A, 0x00,
		0x3D, 0x3C, 0x3B, 0x00,
		0x3E, 0x3D, 0x3C, 0x00,
		0x3F, 0x3E, 0x3D, 0x00,
		0x40, 0x3F, 0x3E, 0x00,
		0x41, 0x40, 0x3F, 0x00,
		0x42, 0x41, 0x40, 0x00,
		0x43, 0x42, 0x41, 0x00,
		0x44, 0x43, 0x42, 0x00,
		0x45, 0x44, 0x43, 0x00,
		0x46, 0x45, 0x44, 0x00,
		0x47, 0x46, 0x45, 0x00,
		0x48, 0x47, 0x46, 0x00,
		0x49, 0x48, 0x47, 0x00,
		0x4A, 0x49, 0x48, 0x00,
		0x4B, 0x4A, 0x49, 0x00,
		0x4C, 0x4B, 0x4A, 0x00,
		0x4D, 0x4C, 0x4B, 0x00,
		0x4E, 0x4D, 0x4C, 0x00,
		0x4F, 0x4E, 0x4D, 0x00,
		0x50, 0x4F, 0x4E, 0x00,
		0x51, 0x50, 0x4F, 0x00,
		0x52, 0x51, 0x50, 0x00,
		0x53, 0x52, 0x51, 0x00,
		0x54, 0x53, 0x52, 0x00,
		0x55, 0x54, 0x53, 0x00,
		0x56, 0x55, 0x54, 0x00,
		0x57, 0x56, 0x55, 0x00,
		0x58, 0x57, 0x56, 0x00,
		0x59, 0x58, 0x57, 0x00,
		0x5A, 0x59, 0x58, 0x00,
		0x5B, 0x5A, 0x59, 0x00,
		0x5C, 0x5B, 0x5A, 0x00,
		0x5D, 0x5C, 0x5B, 0x00,
		0x5E, 0x5D, 0x5C, 0x00,
		0x5F, 0x5E, 0x5D, 0x00,
		0x60, 0x5F, 0x5E, 0x00,
		0x61, 0x60, 0x5F, 0x00,
		0x62, 0x61, 0x60, 0x00,
		0x63, 0x62, 0x61, 0x00,
		0x64, 0x63, 0x62, 0x00,
		0x65, 0x64, 0x63, 0x00,
		0x66, 0x65, 0x64, 0x00,
		0x67, 0x66, 0x65, 0x00,
		0x68, 0x67, 0x66, 0x00,
		0x69, 0x68, 0x67, 0x00,
		0x6A, 0x69, 0x68, 0x00,
		0x6B, 0x6A, 0x69, 0x00,
		0x6C, 0x6B, 0x6A, 0x00,
		0x6D, 0x6C, 0x6B, 0x00,
		0x6E, 0x6D, 0x6C, 0x00,
		0x6F, 0x6E, 0x6D, 0x00,
		0x70, 0x6F, 0x6E, 0x00,
		0x71, 0x70, 0x6F, 0x00,
		0x72, 0x71, 0x70, 0x00,
		0x73, 0x72, 0x71, 0x00,
		0x74, 0x73, 0x72, 0x00,
		0x75, 0x74, 0x73, 0x00,
		0x76, 0x75, 0x74, 0x00,
		0x77, 0x76, 0x75, 0x00,
		0x78, 0x77, 0x76, 0x00,
		0x79, 0x78, 0x77, 0x00,
		0x7A, 0x79, 0x78, 0x00,
		0x7B, 0x7A, 0x79, 0x00,
		0x7C, 0x7B, 0x7A, 0x00,
		0x7D, 0x7C, 0x7B, 0x00,
		0x7E, 0x7D, 0x7C, 0x00,
		0x7F, 0x7E, 0x7D, 0x00,
		0x80, 0x7F, 0x7E, 0x00,
		0x81, 0x80, 0x7F, 0x00,
		0x82, 0x81, 0x80, 0x00,
		0x83, 0x82, 0x81, 0x00,
		0x84, 0x83, 0x82, 0x00,
		0x85, 0x84, 0x83, 0x00,
		0x86, 0x85, 0x84, 0x00,
		0x87, 0x86, 0x85, 0x00,
		0x88, 0x87, 0x86, 0x00,
		0x89, 0x88, 0x87, 0x00,
		0x8A, 0x89, 0x88, 0x00,
		0x8B, 0x8A, 0x89, 0x00,
		0x8C, 0x8B, 0x8A, 0x00,
		0x8D, 0x8C, 0x8B, 0x00,
		0x8E, 0x8D, 0x8C, 0x00,
		0x8F, 0x8E, 0x8D, 0x00,
		0x90, 0x8F, 0x8E, 0x00,
		0x91, 0x90, 0x8F, 0x00,
		0x92, 0x91, 0x90, 0x00,
		0x93, 0x92, 0x91, 0x00,
		0x94, 0x93, 0x92, 0x00,
		0x95, 0x94, 0x93, 0x00,
		0x96, 0x95, 0x94, 0x00,
		0x97, 0x96, 0x95, 0x00,
		0x98, 0x97, 0x96, 0x00,
		0x99, 0x98, 0x97, 0x00,
		0x9A, 0x99, 0x98, 0x00,
		0x9B, 0x9A, 0x99, 0x00,
		0x9C, 0x9B, 0x9A, 0x00,
		0x9D, 0x9C, 0x9B, 0x00,
		0x9E, 0x9D, 0x9C, 0x00,
		0x9F, 0x9E, 0x9D, 0x00,
		0xA0, 0x9F, 0x9E, 0x00,
		0xA1, 0xA0, 0x9F, 0x00,
		0xA2, 0xA1, 0xA0, 0x00,
		0xA3, 0xA2, 0xA1, 0x00,
		0xA4, 0xA3, 0xA2, 0x00,
		0xA5, 0xA4, 0xA3, 0x00,
		0xA6, 0xA5, 0xA4, 0x00,
		0xA7, 0xA6, 0xA5, 0x00,
		0xA8, 0xA7, 0xA6, 0x00,
		0xA9, 0xA8, 0xA7, 0x00,
		0xAA, 0xA9, 0xA8, 0x00,
		0xAB, 0xAA, 0xA9, 0x00,
		0xAC, 0xAB, 0xAA, 0x00,
		0xAD, 0xAC, 0xAB, 0x00,
		0xAE, 0xAD, 0xAC, 0x00,
		0xAF, 0xAE, 0xAD, 0x00,
		0xB0, 0xAF, 0xAE, 0x00,
		0xB1, 0xB0, 0xAF, 0x00,
		0xB2, 0xB1, 0xB0, 0x00,
		0xB3, 0xB2, 0xB1, 0x00,
		0xB4, 0xB3, 0xB2, 0x00,
		0xB5, 0xB4, 0xB3, 0x00,
		0xB6, 0xB5, 0xB4, 0x00,
		0xB7, 0xB6, 0xB5, 0x00,
		0xB8, 0xB7, 0xB6, 0x00,
		0xB9, 0xB8, 0xB7, 0x00,
		0xBA, 0xB9, 0xB8, 0x00,
		0xBB, 0xBA, 0xB9, 0x00,
		0xBC, 0xBB, 0xBA, 0x00,
		0xBD, 0xBC, 0xBB, 0x00,
		0xBE, 0xBD, 0xBC, 0x00,
		0xBF, 0xBE, 0xBD, 0x00,
		0xC0, 0xBF, 0xBE, 0x00,
		0xC1, 0xC0, 0xBF, 0x00,
		0xC2, 0xC1, 0xC0, 0x00,
		0xC3, 0xC2, 0xC1, 0x00,
		0xC4, 0xC3, 0xC2, 0x00,
		0xC5, 0xC4, 0xC3, 0x00,
		0xC6, 0xC5, 0xC4, 0x00,
		0xC7, 0xC6, 0xC5, 0x00,
		0xC8, 0xC7, 0xC6, 0x00,
		0xC9, 0xC8, 0xC7, 0x00,
		0xCA, 0xC9, 0xC8, 0x00,
		0xCB, 0xCA, 0xC9, 0x00,
		0xCC, 0xCB, 0xCA, 0x00,
		0xCD, 0xCC, 0xCB, 0x00,
		0xCE, 0xCD, 0xCC, 0x00,
		0xCF, 0xCE, 0xCD, 0x00,
		0xD0, 0xCF, 0xCE, 0x00,
		0xD1, 0xD0, 0xCF, 0x00,
		0xD2, 0xD1, 0xD0, 0x00,
		0xD3, 0xD2, 0xD1, 0x00,
		0xD4, 0xD3, 0xD2, 0x00,
		0xD5, 0xD4, 0xD3, 0x00,
		0xD6, 0xD5, 0xD4, 0x00,
		0xD7, 0xD6, 0xD5, 0x00,
		0xD8, 0xD7, 0xD6, 0x00,
		0xD9, 0xD8, 0xD7, 0x00,
		0xDA, 0xD9, 0xD8, 0x00,
		0xDB, 0xDA, 0xD9, 0x00,
		0xDC, 0xDB, 0xDA, 0x00,
		0xDD, 0xDC, 0xDB, 0x00,
		0xDE, 0xDD, 0xDC, 0x00,
		0xDF, 0xDE, 0xDD, 0x00,
		0xE0, 0xDF, 0xDE, 0x00,
		0xE1, 0xE0, 0xDF, 0x00,
		0xE2, 0xE1, 0xE0, 0x00,
		0xE3, 0xE2, 0xE1, 0x00,
		0xE4, 0xE3, 0xE2, 0x00,
		0xE5, 0xE4, 0xE3, 0x00,
		0xE6, 0xE5, 0xE4, 0x00,
		0xE7, 0xE6, 0xE5, 0x00,
		0xE8, 0xE7, 0xE6, 0x00,
		0xE9, 0xE8, 0xE7, 0x00,
		0xEA, 0xE9, 0xE8, 0x00,
		0xEB, 0xEA, 0xE9, 0x00,
		0xEC, 0xEB, 0xEA, 0x00,
		0xED, 0xEC, 0xEB, 0x00,
		0xEE, 0xED, 0xEC, 0x00,
		0xEF, 0xEE, 0xED, 0x00,
		0xF0, 0xEF, 0xEE, 0x00,
		0xF1, 0xF0, 0xEF, 0x00,
		0xF2, 0xF1, 0xF0, 0x00,
		0xF3, 0xF2, 0xF1, 0x00,
		0xF4, 0xF3, 0xF2, 0x00,
		0xF5, 0xF4, 0xF3, 0x00,
		0xF6, 0xF5, 0xF4, 0x00,
		0xF7, 0xF6, 0xF5, 0x00,
		0xF8, 0xF7, 0xF6, 0x00,
		0xF9, 0xF8, 0xF7, 0x00,
		0xFA, 0xF9, 0xF8, 0x00,
		0xFB, 0xFA, 0xF9, 0x00,
		0xFC, 0xFB, 0xFA, 0x00,
		0xFD, 0xFC, 0xFB, 0x00,
		0xFE, 0xFD, 0xFC, 0x00,
		0xFF, 0xFE, 0xFD, 0x00,
	};
	static const uint8_t body_a[] = {
		0x12, 0x34, 0x56, 0x78, 0x9a,
	};
	static const uint8_t body_b[] = {
		0xbc, 0xde, 0xf0, 0x12, 0x34,
	};
	static const uint8_t padding[] = {
		0x00, 0x00, 0x00,
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = palette, .nb_bytes = LIS_COUNT_OF(palette) },
		{ .content = body_a, .nb_bytes = LIS_COUNT_OF(body_a) },
		{ .content = padding, .nb_bytes = LIS_COUNT_OF(padding) },
		{ .content = body_b, .nb_bytes = LIS_COUNT_OF(body_b) },
		{ .content = padding, .nb_bytes = LIS_COUNT_OF(padding) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row N-1 (reversed)
	LIS_ASSERT_EQUAL(buffer[0], 0x98);
	LIS_ASSERT_EQUAL(buffer[1], 0x99);
	LIS_ASSERT_EQUAL(buffer[2], 0x9a);

	LIS_ASSERT_EQUAL(buffer[3], 0x76);
	LIS_ASSERT_EQUAL(buffer[4], 0x77);
	LIS_ASSERT_EQUAL(buffer[5], 0x78);

	LIS_ASSERT_EQUAL(buffer[6], 0x54);
	LIS_ASSERT_EQUAL(buffer[7], 0x55);
	LIS_ASSERT_EQUAL(buffer[8], 0x56);

	LIS_ASSERT_EQUAL(buffer[9], 0x32);
	LIS_ASSERT_EQUAL(buffer[10], 0x33);
	LIS_ASSERT_EQUAL(buffer[11], 0x34);

	LIS_ASSERT_EQUAL(buffer[12], 0x10);
	LIS_ASSERT_EQUAL(buffer[13], 0x11);
	LIS_ASSERT_EQUAL(buffer[14], 0x12);

	// row N-2 (reversed)
	LIS_ASSERT_EQUAL(buffer[15], 0x32);
	LIS_ASSERT_EQUAL(buffer[16], 0x33);
	LIS_ASSERT_EQUAL(buffer[17], 0x34);

	LIS_ASSERT_EQUAL(buffer[18], 0x10);
	LIS_ASSERT_EQUAL(buffer[19], 0x11);
	LIS_ASSERT_EQUAL(buffer[20], 0x12);

	LIS_ASSERT_EQUAL(buffer[21], 0xee);
	LIS_ASSERT_EQUAL(buffer[22], 0xef);
	LIS_ASSERT_EQUAL(buffer[23], 0xf0);

	LIS_ASSERT_EQUAL(buffer[24], 0xdc);
	LIS_ASSERT_EQUAL(buffer[25], 0xdd);
	LIS_ASSERT_EQUAL(buffer[26], 0xde);

	LIS_ASSERT_EQUAL(buffer[27], 0xba);
	LIS_ASSERT_EQUAL(buffer[28], 0xbb);
	LIS_ASSERT_EQUAL(buffer[29], 0xbc);

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_bmp2raw_8_no_palette(void)
{
	// BMP/DIB with depth <= 8 bits should always have a palette
	// however it seems that some scanner/driver do not add one.

	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x46, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x08, 0x00, // number of bits per pixels
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x10, 0x00, 0x00, 0x00, // size of pixel data (5px + padding) * 2px
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t body_a[] = {
		0x12, 0x34, 0x56, 0x78, 0x9a,
	};
	static const uint8_t body_b[] = {
		0xbc, 0xde, 0xf0, 0x12, 0x34,
	};
	static const uint8_t padding[] = {
		0x00, 0x00, 0x00,
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = body_a, .nb_bytes = LIS_COUNT_OF(body_a) },
		{ .content = padding, .nb_bytes = LIS_COUNT_OF(padding) },
		{ .content = body_b, .nb_bytes = LIS_COUNT_OF(body_b) },
		{ .content = padding, .nb_bytes = LIS_COUNT_OF(padding) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row N-1 (reversed)
	LIS_ASSERT_EQUAL(buffer[0], 0x9a);
	LIS_ASSERT_EQUAL(buffer[1], 0x9a);
	LIS_ASSERT_EQUAL(buffer[2], 0x9a);

	LIS_ASSERT_EQUAL(buffer[3], 0x78);
	LIS_ASSERT_EQUAL(buffer[4], 0x78);
	LIS_ASSERT_EQUAL(buffer[5], 0x78);

	LIS_ASSERT_EQUAL(buffer[6], 0x56);
	LIS_ASSERT_EQUAL(buffer[7], 0x56);
	LIS_ASSERT_EQUAL(buffer[8], 0x56);

	LIS_ASSERT_EQUAL(buffer[9], 0x34);
	LIS_ASSERT_EQUAL(buffer[10], 0x34);
	LIS_ASSERT_EQUAL(buffer[11], 0x34);

	LIS_ASSERT_EQUAL(buffer[12], 0x12);
	LIS_ASSERT_EQUAL(buffer[13], 0x12);
	LIS_ASSERT_EQUAL(buffer[14], 0x12);

	// row N-2 (reversed)
	LIS_ASSERT_EQUAL(buffer[15], 0x34);
	LIS_ASSERT_EQUAL(buffer[16], 0x34);
	LIS_ASSERT_EQUAL(buffer[17], 0x34);

	LIS_ASSERT_EQUAL(buffer[18], 0x12);
	LIS_ASSERT_EQUAL(buffer[19], 0x12);
	LIS_ASSERT_EQUAL(buffer[20], 0x12);

	LIS_ASSERT_EQUAL(buffer[21], 0xf0);
	LIS_ASSERT_EQUAL(buffer[22], 0xf0);
	LIS_ASSERT_EQUAL(buffer[23], 0xf0);

	LIS_ASSERT_EQUAL(buffer[24], 0xde);
	LIS_ASSERT_EQUAL(buffer[25], 0xde);
	LIS_ASSERT_EQUAL(buffer[26], 0xde);

	LIS_ASSERT_EQUAL(buffer[27], 0xbc);
	LIS_ASSERT_EQUAL(buffer[28], 0xbc);
	LIS_ASSERT_EQUAL(buffer[29], 0xbc);

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_bmp2raw_1(void)
{
	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x46, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x3e, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x01, 0x00, // number of bits per pixels
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x10, 0x00, 0x00, 0x00, // size of pixel data (5px + padding) * 2px
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x02, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t palette[] = {
		// use a weird palette to make sure we didn't fall back to
		// a default one
		0x01, 0x02, 0x03, 0x00,
		0x04, 0x05, 0x06, 0x00
	};
	static const uint8_t body[] = {
		0x15, 0x0, 0x0, 0x0, // 0b10101 + padding
		0xA, 0x0, 0x0, 0x0, // 0b01010 + padding
	};
	static const struct lis_dumb_read reads[] = {
		{ .content = header_a, .nb_bytes = LIS_COUNT_OF(header_a) },
		{ .content = header_b, .nb_bytes = LIS_COUNT_OF(header_b) },
		{ .content = palette, .nb_bytes = LIS_COUNT_OF(palette) },
		{ .content = body, .nb_bytes = LIS_COUNT_OF(body) },
	};

	enum lis_error err;
	struct lis_item *item;
	struct lis_scan_session *session;
	struct lis_scan_parameters params;
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row N-1 (reversed)
	LIS_ASSERT_EQUAL(buffer[0], 0x06);
	LIS_ASSERT_EQUAL(buffer[1], 0x05);
	LIS_ASSERT_EQUAL(buffer[2], 0x04);

	LIS_ASSERT_EQUAL(buffer[3], 0x03);
	LIS_ASSERT_EQUAL(buffer[4], 0x02);
	LIS_ASSERT_EQUAL(buffer[5], 0x01);

	LIS_ASSERT_EQUAL(buffer[6], 0x06);
	LIS_ASSERT_EQUAL(buffer[7], 0x05);
	LIS_ASSERT_EQUAL(buffer[8], 0x04);

	LIS_ASSERT_EQUAL(buffer[9], 0x03);
	LIS_ASSERT_EQUAL(buffer[10], 0x02);
	LIS_ASSERT_EQUAL(buffer[11], 0x01);

	LIS_ASSERT_EQUAL(buffer[12], 0x06);
	LIS_ASSERT_EQUAL(buffer[13], 0x05);
	LIS_ASSERT_EQUAL(buffer[14], 0x04);

	// row N-2 (reversed)
	LIS_ASSERT_EQUAL(buffer[15], 0x03);
	LIS_ASSERT_EQUAL(buffer[16], 0x02);
	LIS_ASSERT_EQUAL(buffer[17], 0x01);

	LIS_ASSERT_EQUAL(buffer[18], 0x06);
	LIS_ASSERT_EQUAL(buffer[19], 0x05);
	LIS_ASSERT_EQUAL(buffer[20], 0x04);

	LIS_ASSERT_EQUAL(buffer[21], 0x03);
	LIS_ASSERT_EQUAL(buffer[22], 0x02);
	LIS_ASSERT_EQUAL(buffer[23], 0x01);

	LIS_ASSERT_EQUAL(buffer[24], 0x06);
	LIS_ASSERT_EQUAL(buffer[25], 0x05);
	LIS_ASSERT_EQUAL(buffer[26], 0x04);

	LIS_ASSERT_EQUAL(buffer[27], 0x03);
	LIS_ASSERT_EQUAL(buffer[28], 0x02);
	LIS_ASSERT_EQUAL(buffer[29], 0x01);

	LIS_ASSERT_TRUE(session->end_of_feed(session));
	LIS_ASSERT_TRUE(session->end_of_page(session));
	session->cancel(session);

	item->close(item);

	LIS_ASSERT_EQUAL(tests_raw_clean(), 0);
}


static void tests_bmp2raw_1_no_palette(void)
{
	// BMP/DIB with depth <= 8 bits should always have a palette
	// however it seems that some scanner/driver do not add one.

	static const struct lis_scan_parameters base_scan_params = {
		.format = LIS_IMG_FORMAT_BMP,
		.width = 2222,
		.height = 2222,
		.image_size = 22222222,
	};
	static const uint8_t header_a[] = {
		0x42, 0x4d, // 'B', 'M' (magic)
		0x3e, 0x00, 0x00, 0x00, // total number of bytes
		0x00, 0x00, 0x00, 0x00, // unused
		0x36, 0x00, 0x00, 0x00, // offset to start of pixel data
		0x28, 0x00, 0x00, 0x00, // number of bytes remaining in header
		0x05, 0x00, 0x00, 0x00, // width
		0x02, 0x00, // height (1st part)
	};
	static const uint8_t header_b[] = {
		0x00, 0x00, // height (2nd part)
		0x01, 0x00, // number of color planes
		0x01, 0x00, // number of bits per pixels
		0x00, 0x00, 0x00, 0x00, // compression (none)
		0x10, 0x00, 0x00, 0x00, // size of pixel data (5px + padding) * 2px
		0x00, 0x00, 0x00, 0x00, // horizontal resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // vertical resolution (pixels/m)
		0x00, 0x00, 0x00, 0x00, // number of colors in palette
		0x00, 0x00, 0x00, 0x00, // important colors
	};
	static const uint8_t body[] = {
		0x15, 0x0, 0x0, 0x0, // 0b10101 + padding
		0xA, 0x0, 0x0, 0x0, // 0b01010 + padding
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
	size_t bufsize, r;
	uint8_t buffer[2 * 5 * 3];

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
	LIS_ASSERT_EQUAL(params.width, 5);
	LIS_ASSERT_EQUAL(params.height, 2);
	LIS_ASSERT_EQUAL(params.image_size, 2 * 5 * 3);

	LIS_ASSERT_FALSE(session->end_of_feed(session));
	bufsize = 0;
	while(bufsize < sizeof(buffer)) {
		LIS_ASSERT_FALSE(session->end_of_page(session));
		r = sizeof(buffer) - bufsize;
		err = session->scan_read(session, buffer + bufsize, &r);
		LIS_ASSERT_EQUAL(err, LIS_OK);
		bufsize += r;
	}
	LIS_ASSERT_EQUAL(bufsize, sizeof(buffer));

	// row N-1 (reversed)
	LIS_ASSERT_EQUAL(buffer[0], 0xFF);
	LIS_ASSERT_EQUAL(buffer[1], 0xFF);
	LIS_ASSERT_EQUAL(buffer[2], 0xFF);

	LIS_ASSERT_EQUAL(buffer[3], 0x00);
	LIS_ASSERT_EQUAL(buffer[4], 0x00);
	LIS_ASSERT_EQUAL(buffer[5], 0x00);

	LIS_ASSERT_EQUAL(buffer[6], 0xFF);
	LIS_ASSERT_EQUAL(buffer[7], 0xFF);
	LIS_ASSERT_EQUAL(buffer[8], 0xFF);

	LIS_ASSERT_EQUAL(buffer[9], 0x00);
	LIS_ASSERT_EQUAL(buffer[10], 0x00);
	LIS_ASSERT_EQUAL(buffer[11], 0x00);

	LIS_ASSERT_EQUAL(buffer[12], 0xFF);
	LIS_ASSERT_EQUAL(buffer[13], 0xFF);
	LIS_ASSERT_EQUAL(buffer[14], 0xFF);

	// row N-2 (reversed)
	LIS_ASSERT_EQUAL(buffer[15], 0x00);
	LIS_ASSERT_EQUAL(buffer[16], 0x00);
	LIS_ASSERT_EQUAL(buffer[17], 0x00);

	LIS_ASSERT_EQUAL(buffer[18], 0xFF);
	LIS_ASSERT_EQUAL(buffer[19], 0xFF);
	LIS_ASSERT_EQUAL(buffer[20], 0xFF);

	LIS_ASSERT_EQUAL(buffer[21], 0x00);
	LIS_ASSERT_EQUAL(buffer[22], 0x00);
	LIS_ASSERT_EQUAL(buffer[23], 0x00);

	LIS_ASSERT_EQUAL(buffer[24], 0xFF);
	LIS_ASSERT_EQUAL(buffer[25], 0xFF);
	LIS_ASSERT_EQUAL(buffer[26], 0xFF);

	LIS_ASSERT_EQUAL(buffer[27], 0x00);
	LIS_ASSERT_EQUAL(buffer[28], 0x00);
	LIS_ASSERT_EQUAL(buffer[29], 0x00);

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

	if (CU_add_test(suite, "tests_bmp2raw_24()", tests_bmp2raw_24) == NULL
			|| CU_add_test(suite, "tests_bmp2raw_24_top_to_bottom()",
				tests_bmp2raw_24_top_to_bottom) == NULL
			|| CU_add_test(suite, "tests_bmp2raw_8()", tests_bmp2raw_8) == NULL
			|| CU_add_test(suite, "tests_bmp2raw_8_no_palette()",
				tests_bmp2raw_8_no_palette) == NULL
			|| CU_add_test(suite, "tests_bmp2raw_1()", tests_bmp2raw_1) == NULL
			|| CU_add_test(suite, "tests_bmp2raw_1_no_palette()",
				tests_bmp2raw_1_no_palette) == NULL) {
		fprintf(stderr, "CU_add_test() has failed\n");
		return 0;
	}

	return 1;
}
