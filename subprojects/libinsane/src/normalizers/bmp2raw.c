#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"
#include "../bmp.h"


#define NAME "bmp2raw"


static const unsigned char DEFAULT_PALETTE_1[] = {
	0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0x00
};

static const unsigned char DEFAULT_PALETTE_8[] = {
	0x00, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x01, 0x00,
	0x02, 0x02, 0x02, 0x00,
	0x03, 0x03, 0x03, 0x00,
	0x04, 0x04, 0x04, 0x00,
	0x05, 0x05, 0x05, 0x00,
	0x06, 0x06, 0x06, 0x00,
	0x07, 0x07, 0x07, 0x00,
	0x08, 0x08, 0x08, 0x00,
	0x09, 0x09, 0x09, 0x00,
	0x0A, 0x0A, 0x0A, 0x00,
	0x0B, 0x0B, 0x0B, 0x00,
	0x0C, 0x0C, 0x0C, 0x00,
	0x0D, 0x0D, 0x0D, 0x00,
	0x0E, 0x0E, 0x0E, 0x00,
	0x0F, 0x0F, 0x0F, 0x00,
	0x10, 0x10, 0x10, 0x00,
	0x11, 0x11, 0x11, 0x00,
	0x12, 0x12, 0x12, 0x00,
	0x13, 0x13, 0x13, 0x00,
	0x14, 0x14, 0x14, 0x00,
	0x15, 0x15, 0x15, 0x00,
	0x16, 0x16, 0x16, 0x00,
	0x17, 0x17, 0x17, 0x00,
	0x18, 0x18, 0x18, 0x00,
	0x19, 0x19, 0x19, 0x00,
	0x1A, 0x1A, 0x1A, 0x00,
	0x1B, 0x1B, 0x1B, 0x00,
	0x1C, 0x1C, 0x1C, 0x00,
	0x1D, 0x1D, 0x1D, 0x00,
	0x1E, 0x1E, 0x1E, 0x00,
	0x1F, 0x1F, 0x1F, 0x00,
	0x20, 0x20, 0x20, 0x00,
	0x21, 0x21, 0x21, 0x00,
	0x22, 0x22, 0x22, 0x00,
	0x23, 0x23, 0x23, 0x00,
	0x24, 0x24, 0x24, 0x00,
	0x25, 0x25, 0x25, 0x00,
	0x26, 0x26, 0x26, 0x00,
	0x27, 0x27, 0x27, 0x00,
	0x28, 0x28, 0x28, 0x00,
	0x29, 0x29, 0x29, 0x00,
	0x2A, 0x2A, 0x2A, 0x00,
	0x2B, 0x2B, 0x2B, 0x00,
	0x2C, 0x2C, 0x2C, 0x00,
	0x2D, 0x2D, 0x2D, 0x00,
	0x2E, 0x2E, 0x2E, 0x00,
	0x2F, 0x2F, 0x2F, 0x00,
	0x30, 0x30, 0x30, 0x00,
	0x31, 0x31, 0x31, 0x00,
	0x32, 0x32, 0x32, 0x00,
	0x33, 0x33, 0x33, 0x00,
	0x34, 0x34, 0x34, 0x00,
	0x35, 0x35, 0x35, 0x00,
	0x36, 0x36, 0x36, 0x00,
	0x37, 0x37, 0x37, 0x00,
	0x38, 0x38, 0x38, 0x00,
	0x39, 0x39, 0x39, 0x00,
	0x3A, 0x3A, 0x3A, 0x00,
	0x3B, 0x3B, 0x3B, 0x00,
	0x3C, 0x3C, 0x3C, 0x00,
	0x3D, 0x3D, 0x3D, 0x00,
	0x3E, 0x3E, 0x3E, 0x00,
	0x3F, 0x3F, 0x3F, 0x00,
	0x40, 0x40, 0x40, 0x00,
	0x41, 0x41, 0x41, 0x00,
	0x42, 0x42, 0x42, 0x00,
	0x43, 0x43, 0x43, 0x00,
	0x44, 0x44, 0x44, 0x00,
	0x45, 0x45, 0x45, 0x00,
	0x46, 0x46, 0x46, 0x00,
	0x47, 0x47, 0x47, 0x00,
	0x48, 0x48, 0x48, 0x00,
	0x49, 0x49, 0x49, 0x00,
	0x4A, 0x4A, 0x4A, 0x00,
	0x4B, 0x4B, 0x4B, 0x00,
	0x4C, 0x4C, 0x4C, 0x00,
	0x4D, 0x4D, 0x4D, 0x00,
	0x4E, 0x4E, 0x4E, 0x00,
	0x4F, 0x4F, 0x4F, 0x00,
	0x50, 0x50, 0x50, 0x00,
	0x51, 0x51, 0x51, 0x00,
	0x52, 0x52, 0x52, 0x00,
	0x53, 0x53, 0x53, 0x00,
	0x54, 0x54, 0x54, 0x00,
	0x55, 0x55, 0x55, 0x00,
	0x56, 0x56, 0x56, 0x00,
	0x57, 0x57, 0x57, 0x00,
	0x58, 0x58, 0x58, 0x00,
	0x59, 0x59, 0x59, 0x00,
	0x5A, 0x5A, 0x5A, 0x00,
	0x5B, 0x5B, 0x5B, 0x00,
	0x5C, 0x5C, 0x5C, 0x00,
	0x5D, 0x5D, 0x5D, 0x00,
	0x5E, 0x5E, 0x5E, 0x00,
	0x5F, 0x5F, 0x5F, 0x00,
	0x60, 0x60, 0x60, 0x00,
	0x61, 0x61, 0x61, 0x00,
	0x62, 0x62, 0x62, 0x00,
	0x63, 0x63, 0x63, 0x00,
	0x64, 0x64, 0x64, 0x00,
	0x65, 0x65, 0x65, 0x00,
	0x66, 0x66, 0x66, 0x00,
	0x67, 0x67, 0x67, 0x00,
	0x68, 0x68, 0x68, 0x00,
	0x69, 0x69, 0x69, 0x00,
	0x6A, 0x6A, 0x6A, 0x00,
	0x6B, 0x6B, 0x6B, 0x00,
	0x6C, 0x6C, 0x6C, 0x00,
	0x6D, 0x6D, 0x6D, 0x00,
	0x6E, 0x6E, 0x6E, 0x00,
	0x6F, 0x6F, 0x6F, 0x00,
	0x70, 0x70, 0x70, 0x00,
	0x71, 0x71, 0x71, 0x00,
	0x72, 0x72, 0x72, 0x00,
	0x73, 0x73, 0x73, 0x00,
	0x74, 0x74, 0x74, 0x00,
	0x75, 0x75, 0x75, 0x00,
	0x76, 0x76, 0x76, 0x00,
	0x77, 0x77, 0x77, 0x00,
	0x78, 0x78, 0x78, 0x00,
	0x79, 0x79, 0x79, 0x00,
	0x7A, 0x7A, 0x7A, 0x00,
	0x7B, 0x7B, 0x7B, 0x00,
	0x7C, 0x7C, 0x7C, 0x00,
	0x7D, 0x7D, 0x7D, 0x00,
	0x7E, 0x7E, 0x7E, 0x00,
	0x7F, 0x7F, 0x7F, 0x00,
	0x80, 0x80, 0x80, 0x00,
	0x81, 0x81, 0x81, 0x00,
	0x82, 0x82, 0x82, 0x00,
	0x83, 0x83, 0x83, 0x00,
	0x84, 0x84, 0x84, 0x00,
	0x85, 0x85, 0x85, 0x00,
	0x86, 0x86, 0x86, 0x00,
	0x87, 0x87, 0x87, 0x00,
	0x88, 0x88, 0x88, 0x00,
	0x89, 0x89, 0x89, 0x00,
	0x8A, 0x8A, 0x8A, 0x00,
	0x8B, 0x8B, 0x8B, 0x00,
	0x8C, 0x8C, 0x8C, 0x00,
	0x8D, 0x8D, 0x8D, 0x00,
	0x8E, 0x8E, 0x8E, 0x00,
	0x8F, 0x8F, 0x8F, 0x00,
	0x90, 0x90, 0x90, 0x00,
	0x91, 0x91, 0x91, 0x00,
	0x92, 0x92, 0x92, 0x00,
	0x93, 0x93, 0x93, 0x00,
	0x94, 0x94, 0x94, 0x00,
	0x95, 0x95, 0x95, 0x00,
	0x96, 0x96, 0x96, 0x00,
	0x97, 0x97, 0x97, 0x00,
	0x98, 0x98, 0x98, 0x00,
	0x99, 0x99, 0x99, 0x00,
	0x9A, 0x9A, 0x9A, 0x00,
	0x9B, 0x9B, 0x9B, 0x00,
	0x9C, 0x9C, 0x9C, 0x00,
	0x9D, 0x9D, 0x9D, 0x00,
	0x9E, 0x9E, 0x9E, 0x00,
	0x9F, 0x9F, 0x9F, 0x00,
	0xA0, 0xA0, 0xA0, 0x00,
	0xA1, 0xA1, 0xA1, 0x00,
	0xA2, 0xA2, 0xA2, 0x00,
	0xA3, 0xA3, 0xA3, 0x00,
	0xA4, 0xA4, 0xA4, 0x00,
	0xA5, 0xA5, 0xA5, 0x00,
	0xA6, 0xA6, 0xA6, 0x00,
	0xA7, 0xA7, 0xA7, 0x00,
	0xA8, 0xA8, 0xA8, 0x00,
	0xA9, 0xA9, 0xA9, 0x00,
	0xAA, 0xAA, 0xAA, 0x00,
	0xAB, 0xAB, 0xAB, 0x00,
	0xAC, 0xAC, 0xAC, 0x00,
	0xAD, 0xAD, 0xAD, 0x00,
	0xAE, 0xAE, 0xAE, 0x00,
	0xAF, 0xAF, 0xAF, 0x00,
	0xB0, 0xB0, 0xB0, 0x00,
	0xB1, 0xB1, 0xB1, 0x00,
	0xB2, 0xB2, 0xB2, 0x00,
	0xB3, 0xB3, 0xB3, 0x00,
	0xB4, 0xB4, 0xB4, 0x00,
	0xB5, 0xB5, 0xB5, 0x00,
	0xB6, 0xB6, 0xB6, 0x00,
	0xB7, 0xB7, 0xB7, 0x00,
	0xB8, 0xB8, 0xB8, 0x00,
	0xB9, 0xB9, 0xB9, 0x00,
	0xBA, 0xBA, 0xBA, 0x00,
	0xBB, 0xBB, 0xBB, 0x00,
	0xBC, 0xBC, 0xBC, 0x00,
	0xBD, 0xBD, 0xBD, 0x00,
	0xBE, 0xBE, 0xBE, 0x00,
	0xBF, 0xBF, 0xBF, 0x00,
	0xC0, 0xC0, 0xC0, 0x00,
	0xC1, 0xC1, 0xC1, 0x00,
	0xC2, 0xC2, 0xC2, 0x00,
	0xC3, 0xC3, 0xC3, 0x00,
	0xC4, 0xC4, 0xC4, 0x00,
	0xC5, 0xC5, 0xC5, 0x00,
	0xC6, 0xC6, 0xC6, 0x00,
	0xC7, 0xC7, 0xC7, 0x00,
	0xC8, 0xC8, 0xC8, 0x00,
	0xC9, 0xC9, 0xC9, 0x00,
	0xCA, 0xCA, 0xCA, 0x00,
	0xCB, 0xCB, 0xCB, 0x00,
	0xCC, 0xCC, 0xCC, 0x00,
	0xCD, 0xCD, 0xCD, 0x00,
	0xCE, 0xCE, 0xCE, 0x00,
	0xCF, 0xCF, 0xCF, 0x00,
	0xD0, 0xD0, 0xD0, 0x00,
	0xD1, 0xD1, 0xD1, 0x00,
	0xD2, 0xD2, 0xD2, 0x00,
	0xD3, 0xD3, 0xD3, 0x00,
	0xD4, 0xD4, 0xD4, 0x00,
	0xD5, 0xD5, 0xD5, 0x00,
	0xD6, 0xD6, 0xD6, 0x00,
	0xD7, 0xD7, 0xD7, 0x00,
	0xD8, 0xD8, 0xD8, 0x00,
	0xD9, 0xD9, 0xD9, 0x00,
	0xDA, 0xDA, 0xDA, 0x00,
	0xDB, 0xDB, 0xDB, 0x00,
	0xDC, 0xDC, 0xDC, 0x00,
	0xDD, 0xDD, 0xDD, 0x00,
	0xDE, 0xDE, 0xDE, 0x00,
	0xDF, 0xDF, 0xDF, 0x00,
	0xE0, 0xE0, 0xE0, 0x00,
	0xE1, 0xE1, 0xE1, 0x00,
	0xE2, 0xE2, 0xE2, 0x00,
	0xE3, 0xE3, 0xE3, 0x00,
	0xE4, 0xE4, 0xE4, 0x00,
	0xE5, 0xE5, 0xE5, 0x00,
	0xE6, 0xE6, 0xE6, 0x00,
	0xE7, 0xE7, 0xE7, 0x00,
	0xE8, 0xE8, 0xE8, 0x00,
	0xE9, 0xE9, 0xE9, 0x00,
	0xEA, 0xEA, 0xEA, 0x00,
	0xEB, 0xEB, 0xEB, 0x00,
	0xEC, 0xEC, 0xEC, 0x00,
	0xED, 0xED, 0xED, 0x00,
	0xEE, 0xEE, 0xEE, 0x00,
	0xEF, 0xEF, 0xEF, 0x00,
	0xF0, 0xF0, 0xF0, 0x00,
	0xF1, 0xF1, 0xF1, 0x00,
	0xF2, 0xF2, 0xF2, 0x00,
	0xF3, 0xF3, 0xF3, 0x00,
	0xF4, 0xF4, 0xF4, 0x00,
	0xF5, 0xF5, 0xF5, 0x00,
	0xF6, 0xF6, 0xF6, 0x00,
	0xF7, 0xF7, 0xF7, 0x00,
	0xF8, 0xF8, 0xF8, 0x00,
	0xF9, 0xF9, 0xF9, 0x00,
	0xFA, 0xFA, 0xFA, 0x00,
	0xFB, 0xFB, 0xFB, 0x00,
	0xFC, 0xFC, 0xFC, 0x00,
	0xFD, 0xFD, 0xFD, 0x00,
	0xFE, 0xFE, 0xFE, 0x00,
	0xFF, 0xFF, 0xFF, 0x00,
};


struct lis_bmp2raw_scan_session;

/**
 * unpack a pixel line in place.
 */
typedef void (bmp_to_raw24_cb)(struct lis_bmp2raw_scan_session *session);

static bmp_to_raw24_cb unpack_1;
static bmp_to_raw24_cb unpack_8;
static bmp_to_raw24_cb unpack_24;


static const struct unpack_rule
{
	int depth;
	const unsigned char *default_palette;
	int default_palette_len;
	bmp_to_raw24_cb *unpack_cb;
} g_unpack_rules[] = {
	{
		.depth = 1,
		.default_palette = DEFAULT_PALETTE_1,
		.default_palette_len = LIS_COUNT_OF(DEFAULT_PALETTE_1) / 4,
		.unpack_cb = unpack_1,
	},
	{
		.depth = 8,
		.default_palette = DEFAULT_PALETTE_8,
		.default_palette_len = LIS_COUNT_OF(DEFAULT_PALETTE_8) / 4,
		.unpack_cb = unpack_8,
	},
	{
		.depth = 24,
		.default_palette = NULL,
		.default_palette_len = 0,
		.unpack_cb = unpack_24,
	},
};


static enum lis_error lis_bmp2raw_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *params
);
static int lis_bmp2raw_end_of_feed(struct lis_scan_session *session);
static int lis_bmp2raw_end_of_page(struct lis_scan_session *session);
static enum lis_error lis_bmp2raw_scan_read(
	struct lis_scan_session *session, void *out_buffer,
	size_t *buffer_size
);
static void lis_bmp2raw_cancel(struct lis_scan_session *session);


struct lis_bmp2raw_scan_session
{
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_item *item;

	bool header_read;
	struct lis_scan_parameters parameters_wrapped;
	struct lis_scan_parameters parameters_out;
	int need_mirroring;

	const struct unpack_rule *unpack;
	unsigned char *palette;
	unsigned int palette_len;

	enum lis_error read_err;

	struct {
		struct {
			int useful; // useful part of the line
			int padding; // extra padding at the end of each line
		} packed;

		struct {
			int useful; // useful part of the line
			int current; // what has been read in the current line
		} unpacked;

		uint8_t *content;
	} line;
};
#define LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session) \
	((struct lis_bmp2raw_scan_session *)(session))


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = lis_bmp2raw_get_scan_parameters,
	.end_of_feed = lis_bmp2raw_end_of_feed,
	.end_of_page = lis_bmp2raw_end_of_page,
	.scan_read = lis_bmp2raw_scan_read,
	.cancel = lis_bmp2raw_cancel,
};


static enum lis_error scan_read_bmp_header(
		struct lis_scan_session *session,
		unsigned char *out, size_t bufsize
	)
{
	size_t nb = 0;
	enum lis_error err;

	assert(bufsize > 0);

	while(bufsize > 0 && !session->end_of_page(session)) {
		nb = bufsize;
		err = session->scan_read(session, out, &nb);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to read BMP header: 0x%X, %s"
				" (already read: %lu B)",
				err, lis_strerror(err),
				(long unsigned)nb
			);
			return err;
		}

		bufsize -= nb;
		out += nb;
	}

	if (bufsize > 0) {
		lis_log_error(
			"Failed to read BMP header: unexpected EOF"
			" (remaining: %lu B)",
			(long unsigned)bufsize
		);
		return LIS_ERR_IO_ERROR;
	}
	return LIS_OK;
}


static enum lis_error read_bmp_header(struct lis_bmp2raw_scan_session *private)
{
	enum lis_error err;
	unsigned char buffer[BMP_HEADER_SIZE];
	size_t h, nb;
	int depth;
	unsigned int i;

	FREE(private->line.content);
	private->line.unpacked.current = 0;
	private->line.unpacked.useful = 0;
	private->line.packed.padding = 0;
	private->line.packed.useful = 0;
	FREE(private->palette);
	private->palette_len = 0;

	memset(&private->parameters_wrapped, 0, sizeof(private->parameters_wrapped));
	memset(&private->parameters_out, 0, sizeof(private->parameters_out));
	err = private->wrapped->get_scan_parameters(
		private->wrapped, &private->parameters_wrapped
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (private->parameters_wrapped.format != LIS_IMG_FORMAT_BMP) {
		lis_log_warning(
			"Unexpected image format: %d. Returning it as is",
			private->parameters_wrapped.format
		);
		return LIS_OK;
	}

	err = scan_read_bmp_header(private->wrapped, buffer, sizeof(buffer));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	h = sizeof(buffer);
	err = lis_bmp2scan_params(
		buffer, &h, &private->parameters_out,
		&depth, &private->palette_len
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	for (i = 0 ; i < LIS_COUNT_OF(g_unpack_rules) ; i++) {
		if (depth == g_unpack_rules[i].depth) {
			private->unpack = &g_unpack_rules[i];
			break;
		}
	}
	if (i >= LIS_COUNT_OF(g_unpack_rules)) {
		lis_log_error("Unknown bits per pixel value: %d", depth);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	if (private->parameters_out.height >= 0) {
		// by default, BMP are bottom-to-top, but our RAW24 goes
		// top-to-bottom. So to fix it and keep the scan displayable
		// on-the-fly, we reverse the lines (--> page will appear
		// rotated 180 degrees).
		private->need_mirroring = 1;
	} else {
		private->parameters_out.height *= -1;
	}

	// line length we want in the output
	private->line.packed.useful = (
		private->parameters_out.width * depth / 8
	);
	if (depth % 8 != 0) {
		private->line.packed.useful += 1;
	}
	// we unpack into raw24, always.
	private->line.unpacked.useful = private->parameters_out.width * 3;

	private->line.packed.padding = 4 - (private->line.packed.useful % 4);
	if (private->line.packed.padding == 4) {
		private->line.packed.padding = 0;
	}
	lis_log_info(
		"[BMP] Line length: %dB + %d (unpacked: %dB)",
		(int)private->line.packed.useful,
		(int)private->line.packed.padding,
		(int)private->line.unpacked.useful
	);

	// we will read line by line ; we need somewhere to store the lines
	i = MAX(
		// line will be unpacked in place
		private->line.packed.useful + private->line.packed.padding,
		private->line.unpacked.useful
	);
	private->line.content = calloc(sizeof(uint8_t), i);
	if (private->line.content == NULL) {
		return LIS_ERR_NO_MEM;
	}

	// mark the current content as used (will force loading the next
	// line next time read() is called)
	private->line.unpacked.current = private->line.unpacked.useful;

	// lis_bmp2scan_params() returns the image size as stored in the BMP
	// but here we want the image size as RAW24
	private->parameters_out.image_size = (
		3 * private->parameters_out.width
		* private->parameters_out.height
	);

	h -= BMP_HEADER_SIZE;

	if (h < private->palette_len * 4) {
		lis_log_error(
			"Inconsistency between the remaining header (%d)"
			" and the palette length (%d * 4 = %dB)",
			(int)h, private->palette_len, private->palette_len * 4
		);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	if (private->palette_len > 0) {

		private->palette = calloc(private->palette_len, 4);
		if (private->palette == NULL) {
			FREE(private->line.content);
			lis_log_error("Failed to allocate memory to store the palette");
			return LIS_ERR_NO_MEM;
		}

		err = scan_read_bmp_header(
			private->wrapped, private->palette, private->palette_len * 4
		);
		if (LIS_IS_ERROR(err)) {
			FREE(private->line.content);
			FREE(private->palette);
			return err;
		}
		h -= private->palette_len * 4;

	} else if (private->unpack->default_palette != NULL) {

		private->palette_len = private->unpack->default_palette_len;
		private->palette = calloc(private->palette_len, 4);
		if (private->palette == NULL) {
			FREE(private->line.content);
			lis_log_error("Failed to allocate memory to store the palette");
			return LIS_ERR_NO_MEM;
		}
		memcpy(
			private->palette, private->unpack->default_palette,
			private->palette_len * 4
		);

	}

	if (h > 0) {
		lis_log_info("Extra BMP header: %lu B", (long unsigned)h);
	}

	while(h > 0) {
		// drop any extra BMP header
		nb = h;
		err = private->wrapped->scan_read(private->wrapped, buffer, &nb);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to read extra BMP header: 0x%X, %s"
				" (remaining to read: %lu B)",
				err, lis_strerror(err), (long unsigned)h
			);
			return err;
		}
		h -= nb;
	}

	return err;
}


static enum lis_error bmp2raw_scan_start(
		struct lis_item *item, struct lis_scan_session **out,
		void *user_data
	)
{
	struct lis_item *original = lis_bw_get_original_item(item);
	struct lis_item *root = lis_bw_get_root_item(item);
	struct lis_bmp2raw_scan_session *private;
	enum lis_error err;

	LIS_UNUSED(user_data);

	private = lis_bw_item_get_user_ptr(root);
	if (private == NULL) {
		FREE(private);
	}

	private = calloc(1, sizeof(struct lis_bmp2raw_scan_session));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	private->read_err = LIS_OK;

	err = original->scan_start(original, &private->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"scan_start() failed: 0x%X, %s",
			err, lis_strerror(err)
		);
		FREE(private);
		return err;
	}
	memcpy(&private->parent, &g_scan_session_template,
		sizeof(private->parent));
	private->item = root;

	err = read_bmp_header(private);
	if (LIS_IS_ERROR(err)) {
		private->wrapped->cancel(private->wrapped);
		FREE(private);
		return err;
	}

	lis_bw_item_set_user_ptr(root, private);
	*out = &private->parent;
	return err;
}


static enum lis_error lis_bmp2raw_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(self);

	memcpy(params, &private->parameters_out, sizeof(*params));
	return LIS_OK;
}


static void bmp2raw_on_item_close(
		struct lis_item *item, int root, void *user_data
	)
{
	struct lis_bmp2raw_scan_session *private;

	LIS_UNUSED(user_data);

	if (!root) {
		return;
	}

	private = lis_bw_item_get_user_ptr(item);
	if (private == NULL) {
		return;
	}

	lis_log_warning(
		"Device has been closed but scan session hasn't been cancelled"
	);
	lis_bmp2raw_cancel(&private->parent);
}


static int lis_bmp2raw_end_of_feed(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	int end_of_feed;
	int end_of_page;

	end_of_page = private->wrapped->end_of_page(private->wrapped);
	end_of_feed = private->wrapped->end_of_feed(private->wrapped);
	if (end_of_feed) {
		FREE(private->line.content);
		return 1;
	}

	if (end_of_page && !private->header_read &&
			!private->parent.end_of_feed(&private->parent)) {
		// we must do it here so get_scan_parameters() will return
		// the correct parameters.
		private->read_err = read_bmp_header(private);
		if (LIS_IS_ERROR(private->read_err)) {
			lis_log_error(
				"Failed to read BMP header: 0x%X, %s",
				private->read_err,
				lis_strerror(private->read_err)
			);
		}
		// avoid double header read
		private->header_read = 1;
	}

	return 0;
}


static int lis_bmp2raw_end_of_page(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);

	if (private->line.unpacked.current < private->line.unpacked.useful) {
		return 0;
	}

	return private->wrapped->end_of_page(private->wrapped);
}


static enum lis_error read_next_line(struct lis_bmp2raw_scan_session *private)
{
	size_t to_read, r;
	enum lis_error err;
	uint8_t *out;

	to_read = private->line.packed.useful + private->line.packed.padding;
	out = private->line.content;
	lis_log_debug("Reading BMP line: %d bytes", (int)to_read);

	while(to_read > 0) {
		r = to_read;
		err = private->wrapped->scan_read(private->wrapped, out, &r);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
		to_read -= r;
		out += r;
	}

	return LIS_OK;
}


static void unpack_1(struct lis_bmp2raw_scan_session *session)
{
	int idx;
	int b;
	int v;
	uint8_t *p;

	assert(session->palette != NULL);
	assert(session->palette_len != 0);

	for (idx = session->parameters_out.width - 1 ; idx >= 0 ; idx--) {
		b = session->line.content[idx / 8];
		v = (b >> (idx % 8)) & 1;
		p = session->palette + (v * 4);

		session->line.content[(idx * 3) + 2] = p[2];
		session->line.content[(idx * 3) + 1] = p[1];
		session->line.content[(idx * 3)] = p[0];
	}
}


static void unpack_8(struct lis_bmp2raw_scan_session *session)
{
	int idx;
	uint8_t v;
	uint8_t *p;

	assert(session->palette != NULL);
	assert(session->palette_len != 0);

	for (idx = session->line.packed.useful - 1 ; idx >= 0 ; idx--) {
		v = session->line.content[idx];
		p = session->palette + (v * 4);

		session->line.content[(idx * 3) + 2] = p[2];
		session->line.content[(idx * 3) + 1] = p[1];
		session->line.content[(idx * 3)] = p[0];
	}
}


static void unpack_24(struct lis_bmp2raw_scan_session *session)
{
	// Nothing to do
	LIS_UNUSED(session);
}


static void bgr2rgb(uint8_t *line, int line_len)
{
	uint8_t tmp;

	for (; line_len > 0 ; line += 3, line_len -= 3) {
		tmp = line[0];
		line[0] = line[2];
		line[2] = tmp;
	}
}


static inline void swap_pixels(uint8_t *pa, uint8_t *pb)
{
	uint8_t tmp[3];

	assert(pa != pb);

	tmp[0] = pa[0];
	tmp[1] = pa[1];
	tmp[2] = pa[2];
	pa[0] = pb[0];
	pa[1] = pb[1];
	pa[2] = pb[2];
	pb[0] = tmp[0];
	pb[1] = tmp[1];
	pb[2] = tmp[2];
}


static void mirror_line(uint8_t *line, int line_len)
{
	int pos;

	for (pos = 0; pos < (line_len / 2) - 3 ; pos += 3) {
		swap_pixels(&line[pos], &line[line_len - pos - 3]);
	}
}


static enum lis_error lis_bmp2raw_scan_read(
		struct lis_scan_session *session,
		void *out_buffer, size_t *buffer_size
	)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	enum lis_error err;
	size_t remaining_to_read = *buffer_size;
	size_t to_copy;

	if (LIS_IS_ERROR(private->read_err)) {
		lis_log_warning(
			"Delayed error: 0x%X, %s",
			private->read_err, lis_strerror(private->read_err)
		);
		return private->read_err;
	}

	// indicate to end_of_feed() that it will have to read again the
	// next header of the next page
	private->header_read = 0;

	while(remaining_to_read > 0) {
		if (private->line.unpacked.current >= private->line.unpacked.useful) {
			if (session->end_of_page(session)) {
				lis_log_debug("scan_read(): end of page");
				*buffer_size -= remaining_to_read;
				return LIS_OK;
			}

			err = read_next_line(private);
			if (LIS_IS_ERROR(err)) {
				lis_log_error(
					"scan_read(): failed to read next"
					" pixel line: 0x%X, %s",
					err, lis_strerror(err)
				);
				return err;
			}

			private->unpack->unpack_cb(private);

			bgr2rgb(
				private->line.content, private->line.unpacked.useful
			);
			if (private->need_mirroring) {
				mirror_line(
					private->line.content,
					private->line.unpacked.useful
				);
			}
			private->line.unpacked.current = 0;
		}

		to_copy = MIN(
			private->line.unpacked.useful - private->line.unpacked.current,
			(int)remaining_to_read
		);
		assert(to_copy > 0);
		memcpy(
			out_buffer,
			private->line.content + private->line.unpacked.current,
			to_copy
		);
		out_buffer = ((uint8_t *)out_buffer) + to_copy;
		remaining_to_read -= to_copy;
		private->line.unpacked.current += to_copy;
	}

	return LIS_OK;
}


static void lis_bmp2raw_cancel(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	FREE(private->palette);
	FREE(private->line.content);
	private->wrapped->cancel(private->wrapped);
	lis_bw_item_set_user_ptr(private->item, NULL);
	FREE(private);
}


enum lis_error lis_api_normalizer_bmp2raw(
		struct lis_api *to_wrap, struct lis_api **api
	)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, api, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	lis_bw_set_on_close_item(*api, bmp2raw_on_item_close, NULL);
	lis_bw_set_on_scan_start(*api, bmp2raw_scan_start, NULL);

	return err;
}
