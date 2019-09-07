#include <stdio.h>
#include <strings.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "workaround_check_caps"


static enum lis_error get_value(struct lis_option_descriptor *self, union lis_value *value)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);

	if (!LIS_OPT_IS_READABLE(original)) {
		lis_log_warning("get_value(%s) -> capabilities prevent getting the value",
			self->name);
		return LIS_ERR_ACCESS_DENIED;
	}

	return original->fn.get_value(original, value);
}


static enum lis_error set_value(struct lis_option_descriptor *self, union lis_value value, int *set_flags)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);

	// WORKAROUND(JFlesch): constraint has only one possible value
	// --> don't try to set it. But still try to keep a consistent behavior.
	if (original->constraint.type == LIS_CONSTRAINT_LIST
			&& original->constraint.possible.list.nb_values == 1) {
		if (lis_compare(original->value.type, value, original->constraint.possible.list.values[0])) {
			lis_log_info("set_value(%s): Only one value possible -> option not set",
				self->name);
			return LIS_OK;
		} else {
			lis_log_warning("set_value(%s) -> only one value possible != different"
				" from value request -> denied", self->name);
			return LIS_ERR_INVALID_VALUE;
		}
	}

	// WORKAROUND(Jflesch): Do not check LIS_OPT_IS_READABLE() here
	// Canon Lide 220 with Sane: option 'source' is marked as INACTIVE (!readable)
	// but SW_SELECT (writable), and normalizers try to write this option.
	if (!LIS_OPT_IS_WRITABLE(original)) {
		lis_log_warning("set_value(%s) -> capabilities prevent setting the value",
			self->name);
		return LIS_ERR_ACCESS_DENIED;
	}

	return original->fn.set_value(original, value, set_flags);
}


static enum lis_error opt_filter(struct lis_item *item, struct lis_option_descriptor *desc, void *user_data)
{
	void *check_inactive; // actually a boolean

	LIS_UNUSED(user_data);

	check_inactive = lis_bw_item_get_user_ptr(item);

	desc->fn.set_value = set_value;
	if (check_inactive) {
		desc->fn.get_value = get_value;
	}
	return LIS_OK;
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	enum lis_error err;
	struct lis_option_descriptor **opts;
	struct lis_item *original_item;
	int i;

	LIS_UNUSED(user_data);
	LIS_UNUSED(root);

	lis_bw_item_set_user_ptr(item, (void*)0x1);

	original_item = lis_bw_get_original_item(item);

	err = original_item->get_options(original_item, &opts);
	if (!LIS_IS_OK(err)) {
		lis_log_warning("Failed to get options. Assuming INACTIVE flags are correctly set");
		return LIS_OK;
	}

	for (i = 0 ; opts[i] != NULL ; i++) {
		if (strcasecmp(opts[i]->name, OPT_NAME_SOURCE) == 0) {
			if (!(opts[i]->capabilities & LIS_CAP_INACTIVE)) {
				lis_log_info(
						"Option 'source' marked as ACTIVE."
						" Assuming flags INACTIVE are correctly set on other options"
						);
				return LIS_OK;
			}

			if (opts[i]->constraint.type != LIS_CONSTRAINT_LIST) {
				lis_log_warning(
						"Unexpected constraint type for option 'source' (%d)."
						" Assuming flags INACTIVE are correctly set on other options",
						opts[i]->constraint.type
						);
				return LIS_OK;
			}

			if (opts[i]->constraint.possible.list.nb_values <= 1) {
				lis_log_warning(
						"Option 'source' has only one possible value."
						" Assuming flags INACTIVE are correctly set on other options"
						);
				return LIS_OK;
			}

			lis_log_warning(
					"Option 'source' is marked INACTIVE but has many possible"
					" values. Assuming the driver doesn't set the flag INACTIVE"
					" correctly."
					);

			lis_bw_item_set_user_ptr(item, NULL);
			return LIS_OK;
		}
	}

	lis_log_warning("Failed to find option 'source'. Assuming INACTIVE flags are correctly set");
	return LIS_OK;
}


enum lis_error lis_api_workaround_check_capabilities(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_item_filter(*impl, item_filter, NULL);
		lis_bw_set_opt_desc_filter(*impl, opt_filter, NULL);
	}

	return err;
}
