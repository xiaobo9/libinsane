#ifndef __LIBINSANE_BASE_WRAPPER_H
#define __LIBINSANE_BASE_WRAPPER_H

#include <libinsane/capi.h>
#include <libinsane/error.h>

/**
 * \brief Base implementation for common wrapping (option value fix, etc)
 */
enum lis_error lis_api_base_wrapper(struct lis_api *to_wrap, struct lis_api **wrapped, const char *wrapped_name);


/**
 * \brief Filter can change item on-the-fly.
 * They can also change callbacks
 * \param[in,out] item item
 * \param[in] root 1 if it is a root item, 0 if it is a children item.
 */
typedef enum lis_error (*lis_bw_item_filter)(struct lis_item *item, int root, void *user_data);
void lis_bw_set_item_filter(struct lis_api *impl, lis_bw_item_filter filter, void *user_data);


/**
 * \brief attach a pointer to the specified item.
 * Item must come from \ref lis_bw_set_item_filter().
 */
void lis_bw_item_set_user_ptr(struct lis_item *item, void *user_ptr);
void *lis_bw_item_get_user_ptr(struct lis_item *item);


/**
 * \brief Returns the original item.
 * \param[in] modified the item, as was passed to the filter \ref lis_bw_item_filter.
 * \return the item without the modifications (do not modify !). NULL if not found.
 */
struct lis_item *lis_bw_get_original_item(struct lis_item *modified);


/**
 * \brief Filter can change option descriptors on-the-fly.
 * They can also change callbacks in option descriptors.
 * \param[in] item item to which belongs this option.
 * \param[in,out] desc option descriptor to filter.
 */
typedef enum lis_error (*lis_bw_opt_desc_filter)(
	const struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
);
void lis_bw_set_opt_desc_filter(struct lis_api *impl, lis_bw_opt_desc_filter filter, void *user_data);

/**
 * \brief attach a pointer to the specified option descriptor.
 * Item must come from \ref lis_bw_set_opt_desc_filter().
 */
void lis_bw_opt_set_user_ptr(struct lis_option_descriptor *opt, void *user_ptr);
void *lis_bw_opt_get_user_ptr(struct lis_option_descriptor *opt);


/**
 * \brief Returns the original option descriptor.
 * \param[in] modified the option descriptor, as was passed to the filter \ref lis_bw_opt_desc_filter.
 * \return the original descriptor without the modifications (do not modify !). NULL if not found.
 */
struct lis_option_descriptor *lis_bw_get_original_opt(struct lis_option_descriptor *modified);


/**
 * \brief called when an API instance is being cleanup/stopped.
 * Note that there maybe many concurrent instances of a same API.
 */
typedef void (*lis_bw_clean_impl)(struct lis_api *impl, void *user_data);
void lis_bw_set_clean_impl(struct lis_api *impl, lis_bw_clean_impl cb, void *user_data);

#endif
