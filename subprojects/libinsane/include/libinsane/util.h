#ifndef __LIBINSANE_UTIL_H
#define __LIBINSANE_UTIL_H

#include "capi.h"
#include "error.h"


#ifdef __cplusplus
extern "C" {
#endif

#define LIS_UNUSED(x) (void)(x)
#define LIS_COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

#ifndef FREE
#define FREE(x) do { \
		free(((void*)(x))); \
		(x) = NULL; \
	} while(0);
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

/*!
 * \brief Copy a value.
 * You *must* \ref lis_free "free the copy" once you're done with it.
 * \param[in] type define the value type.
 * \param[in] original value to copy.
 * \param[out] copy duplicated value.
 */
enum lis_error lis_copy(
	const enum lis_value_type type, const union lis_value *original, union lis_value *copy
);


/*!
 * \brief Free a copied value.
 * \param[in] type define a the value type.
 * \param[in] value value to free. Do not use it after calling this function.
 */
void lis_free(const enum lis_value_type type, union lis_value *value);


/*!
 * \brief helper to set quickly an option
 */
enum lis_error lis_set_option(struct lis_item *item, const char *opt_name, const char *opt_value);


/*!
 * \brief set the scan area to the maximum that the device can do
 */
enum lis_error lis_maximize_scan_area(struct lis_item *item);


/*!
 * \brief compare values
 * \retval 1 if values are identical
 * \retval 0 if values are different
 */
int lis_compare(enum lis_value_type type, union lis_value val1, union lis_value val2);


union lis_value lis_add(enum lis_value_type type, union lis_value a, union lis_value b);
union lis_value lis_sub(enum lis_value_type type, union lis_value a, union lis_value b);

/*!
 * \brief return the value of an environment variable.
 * \param[in] var env variable name
 * \param[in] default_val default value if the variable is not set
 */
int lis_getenv(const char *var, int default_val);

#ifdef __cplusplus
}
#endif

#endif
