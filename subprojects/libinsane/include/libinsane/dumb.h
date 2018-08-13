#ifndef __LIBINSANE_DUMB_H
#define __LIBINSANE_DUMB_H

#include "capi.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIS_DUMB_DEV_ID_FORMAT "dumb dev%d"
#define LIS_DUMB_DEV_ID_FIRST "dumb dev0"

/*!
 * \brief Dumb implementation. Returns 0 scanners. Only useful for testing.
 * Used mostly in unit tests.
 * \param[out] impl will point to the Dumb implementation
 * \param[in] name name of the API
 */
extern enum lis_error lis_api_dumb(struct lis_api **impl, const char *name);

void lis_dumb_set_dev_descs(struct lis_api *impl, struct lis_device_descriptor **descs);

/**
 * \brief generate fake device (and device descriptors)
 */
void lis_dumb_set_nb_devices(struct lis_api *self, int nb_devices);
void lis_dumb_set_list_devices_return(struct lis_api *self, enum lis_error ret);
void lis_dumb_set_get_device_return(struct lis_api *self, enum lis_error ret);

void lis_dumb_add_option(struct lis_api *self, const struct lis_option_descriptor *opt,
	const union lis_value *default_value);

struct lis_dumb_read {
	const char *content;
	size_t nb_bytes;
};

void lis_dumb_set_scan_result(struct lis_api *self, const struct lis_dumb_read *read_contents, int nb_reads);

#ifdef __cplusplus
}
#endif

#endif
