#ifndef __LIBINSANE_STR2IMPL_H
#define __LIBINSANE_STR2IMPL_H

#include "capi.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Converts a string into a set of implementations.
 *
 * Useful for testing. See code for valid strings.
 *
 * API multiplexer is not supported.
 *
 * \param[in] list_of_impls Format: \<base API\>[,\<wrapper\>[,\<wrapper\>[(...)]]]
 * \param[out] impls Requested implementations
 */
extern enum lis_error lis_str2impls(const char *list_of_impls, struct lis_api **impls);

#ifdef __cplusplus
}
#endif

#endif
