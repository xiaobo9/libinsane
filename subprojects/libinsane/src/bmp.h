#ifndef __LIBINSANE_NORMALIZERS_BMP_H
#define __LIBINSANE_NORMALIZERS_BMP_H

#include <stdint.h>

#include <libinsane/capi.h>
#include <libinsane/error.h>

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

#define BMP_HEADER_SIZE sizeof(struct bmp_header)
#define BMP_DIB_HEADER_SIZE 40


/**
 * \brief convert a bitmap into scan parameters
 * \param[in] bmp raw data. Must point to at minimum \ref BMP_HEADER_SIZE bytes
 * \param[out] header_size size of the header (--> number of bytes to ignore
 *  for raw data).
 * \param[out] params corresponding scan parameters, assuming return value
 *  is \ref LIS_IS_OK().
 */
enum lis_error lis_bmp2scan_params(
	const void *bmp,
	size_t *header_size,
	struct lis_scan_parameters *params
);


void lis_scan_params2bmp(
	const struct lis_scan_parameters *params,
	void *header
);

#endif
