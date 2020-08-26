#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "bmp.h"
#include "endianess.h"


enum lis_error lis_bmp2scan_params(
		const void *bmp,
		size_t *header_size,
		struct lis_scan_parameters *params,
		int *depth,
		unsigned int *nb_colors_in_palette
	)
{
	const struct bmp_header *header;

	assert(sizeof(struct bmp_header) == BMP_HEADER_SIZE);

	lis_hexdump("bmp", bmp, BMP_HEADER_SIZE);

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
		lis_log_error(
			"BMP: Don't know how to handle compression: 0x%"PRIX32,
			le32toh(header->compression)
		);
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}

	*depth = le16toh(header->nb_bits_per_pixel);
	if (*depth != 1 && *depth != 8 && *depth != 24) {
		lis_log_error(
			"BMP: Unexpected nb bits per pixel: %u (0x%X)",
			le16toh(header->nb_bits_per_pixel),
			header->nb_bits_per_pixel
		);
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}

	*nb_colors_in_palette = le32toh(header->nb_colors_in_palette);

	params->format = LIS_IMG_FORMAT_RAW_RGB_24;

	params->width = le32toh(header->width);
	// params->image_size = header->file_size - header->offset_to_data;
	params->image_size = le32toh(header->pixel_data_size);

	params->height = le32toh(header->height);
	params->width = le32toh(header->width);

	lis_log_info(
		"BMP header says: %d x %d x %db = %lu",
		params->width, params->height, *depth,
		(long unsigned)params->image_size
	);

	return LIS_OK;
}


void lis_scan_params2bmp(
		const struct lis_scan_parameters *params,
		void *_header,
		int nb_bits_per_pixel
	)
{
	struct bmp_header *header = _header;
	size_t pixel_data;
	size_t line_length;
	size_t padding;

	line_length = params->width * nb_bits_per_pixel / 8;
	if (nb_bits_per_pixel % 8 != 0) {
		line_length += 1;
	}

	padding = 4 - (line_length % 4);
	if (padding == 4) {
		padding = 0;
	}

	memset(header, 0, sizeof(struct bmp_header));
	header->magic = htobe16(0x424D);
	header->offset_to_data = htole32(BMP_HEADER_SIZE);
	header->remaining_header = htole32(0x28);
	header->nb_color_planes = htole16(1);
	header->nb_bits_per_pixel = htole16(nb_bits_per_pixel);
	// we always work from top to bottom: use negative height
	// to indicate that
	header->height = htole32(-1 * params->height);
	header->width = htole32(params->width);

	// padding must be taken into account
	pixel_data = (params->height * (line_length + padding));
	header->pixel_data_size = htole32(pixel_data);
	header->file_size = htole32(BMP_HEADER_SIZE + pixel_data);
}
