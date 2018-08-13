#include <libinsane/capi.h>
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

	if (!LIS_OPT_IS_READABLE(original) || !LIS_OPT_IS_WRITABLE(original)) {
		lis_log_warning("set_value(%s) -> capabilities prevent setting the value",
			self->name);
		return LIS_ERR_ACCESS_DENIED;
	}

	return original->fn.set_value(original, value, set_flags);
}


static enum lis_error opt_filter(struct lis_item *item, struct lis_option_descriptor *desc, void *user_data)
{
	LIS_UNUSED(user_data);
	LIS_UNUSED(item);
	desc->fn.set_value = set_value;
	desc->fn.get_value = get_value;
	return LIS_OK;
}


enum lis_error lis_api_workaround_check_capabilities(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_opt_desc_filter(*impl, opt_filter, NULL);
	}

	return err;
}
