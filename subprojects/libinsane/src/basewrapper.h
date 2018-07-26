#ifndef __LIBINSANE_BASE_WRAPPER_H
#define __LIBINSANE_BASE_WRAPPER_H

#include <libinsane/capi.h>
#include <libinsane/error.h>

/**
 * Base implementation for common wrapping (option value fix, etc)
 */
enum lis_error lis_api_base_wrapper(struct lis_api *to_wrap, struct lis_api **wrapped, const char *wrapped_name);

/**
 * Filter can change option descriptors on-the-fly. They can also change callbacks in
 * option descriptors.
 * \param[in] item item to which belongs this option.
 * \param[in,out] desc option descriptor to filter.
 */
typedef enum lis_error (*lis_bw_opt_desc_filter)(
	const struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
);
void lis_bw_set_opt_desc_filter(struct lis_api *impl, lis_bw_opt_desc_filter filter, void *user_data);

#endif
