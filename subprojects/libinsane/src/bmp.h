#ifndef __LIBINSANE_NORMALIZERS_BMP_H
#define __LIBINSANE_NORMALIZERS_BMP_H

#include <libinsane/capi.h>
#include <libinsane/error.h>

#define BMP_HEADER_SIZE 54

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
