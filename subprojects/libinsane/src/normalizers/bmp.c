#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "bmp.h"

#ifdef OS_LINUX
#include <endian.h>
#else

// assuming Windows x86 --> little endian

#define le32toh(v) (v)

static inline uint16_t be16toh(uint16_t v)
{
	return ((v << 8) | (v >> 8));
}

#endif


struct bmp_header {
	uint16_t magic;
	uint32_t file_size;
	uint32_t unused;
	uint32_t offset_to_data;
	uint32_t remaining_header;
	uint32_t width;
	uint32_t height;
	uint16_t nb_color_planes;
	uint16_t nb_bits_per_pixel;
	uint32_t compression;
	uint32_t pixel_data_size;
	uint32_t horizontal_resolution; // pixels / meter
	uint32_t vertical_resolution; // pixels / meter
	uint32_t nb_colors_in_palette;
	uint32_t important_colors;
} __attribute__((packed));


enum lis_error lis_bmp2scan_params(
		const void *bmp,
		size_t *header_size,
		struct lis_scan_parameters *params
	)
{
	const struct bmp_header *header;

	assert(sizeof(struct bmp_header) == BMP_HEADER_SIZE);

	header = bmp;
	*header_size = le32toh(header->offset_to_data);

	if (be16toh(header->magic) != 0x424D /* "BM" */) {
		lis_log_warning("BMP: Unknown magic header: 0x%"PRIX16, be16toh(header->magic));
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	if (le32toh(header->file_size) < BMP_HEADER_SIZE) {
		lis_log_warning("BMP: File size too small: %u B", le32toh(header->file_size));
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	if (le32toh(header->offset_to_data) < BMP_HEADER_SIZE) {
		lis_log_warning("BMP: Offset to data too small: %u B", le32toh(header->offset_to_data));
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	if (le32toh(header->file_size) < le32toh(header->offset_to_data)) {
		lis_log_warning(
			"BMP: File size smaller than offset to data: %u VS %u",
			le32toh(header->file_size),
			le32toh(header->offset_to_data)
		);
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	if (header->compression != 0) {
		lis_log_error("BMP: Don't know how to handle compression: 0x%"PRIX32, le32toh(header->compression));
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	if (le32toh(header->pixel_data_size) != 24) {
		lis_log_error("BMP: Unexpected pixel data size: %u", le32toh(header->pixel_data_size));
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}

	params->format = LIS_IMG_FORMAT_RAW_RGB_24;
	params->width = le32toh(header->width);
	params->height = le32toh(header->height);
	params->image_size = header->file_size - header->offset_to_data;
	lis_log_info(
		"BMP header says: %d x %d = %lu",
		params->width, params->height,
		(long unsigned)params->image_size
	);

	return LIS_OK;
}

