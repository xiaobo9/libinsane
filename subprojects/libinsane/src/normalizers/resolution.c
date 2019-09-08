#include <assert.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"


#define NAME "normalizer_resolution"
#define MIN_RESOLUTION_INTERVAL 25

static const union lis_value DEFAULT_CONSTRAINT[] = {
	// same range as a simple Brother DS-620
	{ .integer = 75, },
	{ .integer = 100, },
	{ .integer = 125, },
	{ .integer = 150, },
	{ .integer = 175, },
	{ .integer = 200, },
	{ .integer = 225, },
	{ .integer = 250, },
	{ .integer = 275, },
	{ .integer = 300, },
	{ .integer = 325, },
	{ .integer = 350, },
	{ .integer = 375, },
	{ .integer = 400, },
	{ .integer = 425, },
	{ .integer = 450, },
	{ .integer = 475, },
	{ .integer = 500, },
	{ .integer = 525, },
	{ .integer = 550, },
	{ .integer = 575, },
	{ .integer = 600, },
};

static enum lis_error opt_get_value(struct lis_option_descriptor *self, union lis_value *int_value)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);
	union lis_value dbl_value;
	enum lis_error err;

	switch(original->value.type) {
		case LIS_TYPE_INTEGER:
			lis_log_debug("get_value('" OPT_NAME_RESOLUTION "') already has value of type integer");
			/* shouldn't have been called in a first place */
			assert(original->value.type != LIS_TYPE_INTEGER);
			return original->fn.get_value(original, int_value);
		case LIS_TYPE_DOUBLE:
			err = original->fn.get_value(original, &dbl_value);
			lis_log_debug(
				"get_value('" OPT_NAME_RESOLUTION "'): Converting %f into integer",
				dbl_value.dbl
			);
			int_value->integer = (int)(dbl_value.dbl);
			return err;
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
	}
	lis_log_error("Unknown value type: %d", original->value.type);
	assert(0);
	return original->fn.get_value(original, int_value);
}


static enum lis_error opt_set_value(struct lis_option_descriptor *self, union lis_value int_value,
		int *set_flags)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);
	union lis_value dbl_value;

	switch(original->value.type) {
		case LIS_TYPE_INTEGER:
			lis_log_debug("set_value('" OPT_NAME_RESOLUTION "') already has value of type integer");
			/* shouldn't have been called in a first place */
			assert(original->value.type != LIS_TYPE_INTEGER);
			return original->fn.set_value(original, int_value, set_flags);
		case LIS_TYPE_DOUBLE:
			lis_log_debug(
				"set_value('" OPT_NAME_RESOLUTION "'): Converting %d into double",
				int_value.integer
			);
			dbl_value.dbl = int_value.integer;
			return original->fn.set_value(original, dbl_value, set_flags);
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
	}
	lis_log_error("Unknown value type: %d", original->value.type);
	assert(0);
	return original->fn.set_value(original, int_value, set_flags);
}



static enum lis_error fix_range_type(enum lis_value_type type, struct lis_value_range *range)
{
	switch(type) {
		case LIS_TYPE_INTEGER:
			lis_log_debug(
				"Constraint of option '" OPT_NAME_RESOLUTION "'"
				" is already a range of integers"
			);
			return LIS_OK;
		case LIS_TYPE_DOUBLE:
			lis_log_debug(
				"Converting resolution range %f-%f-%f into integers",
				range->min.dbl, range->max.dbl, range->interval.dbl
			);
			range->min.integer = (int)range->min.dbl;
			range->max.integer = (int)range->max.dbl;
			range->interval.integer = (int)range->interval.dbl;
			return LIS_OK;
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
	}
	lis_log_error("Unexpected value types for option '" OPT_NAME_RESOLUTION "' (range): %d", type);
	return LIS_ERR_UNSUPPORTED;
}


static enum lis_error range_to_list(const struct lis_value_range *in, struct lis_value_list *out)
{
	int val, idx;
	int interval;

	interval = in->interval.integer;

	if (interval <= 1) {
		// such interval would generate a list far too long
		interval = MIN_RESOLUTION_INTERVAL;
	}

	// Compute how many values we will generate:
	// Depending on the range, we may add an extra value at the
	// beginning of the list, and another one at the end of the list
	// (--> + 2). We also add an extra slot by safety (--> + 3)
	// out->nb_values will be adjusted later based on the number
	// of values we actually generated.
	out->nb_values = ((in->max.integer - in->min.integer) / interval) + 3;
	out->values = calloc(out->nb_values, sizeof(union lis_value));
	if (out->values == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	idx = 0;
	val = in->min.integer;
	if (in->interval.integer <= 1 && (val % interval) != 0) {
		out->values[idx].integer = val;
		idx++;
		val += interval;
		// realign on the interval for nicer values
		val -= (val % interval);
	}

	for ( ; val <= in->max.integer ; val += interval, idx++) {
		assert(idx < out->nb_values);
		lis_log_debug(
			"Resolution range constraint %d-%d-%d --> list constraint: %d",
			in->min.integer, in->max.integer, interval, val
		);
		out->values[idx].integer = val;
	}

	if (in->interval.integer <= 1 && val != in->max.integer + interval) {
		out->values[idx].integer = in->max.integer;
		idx++;
	}

	out->nb_values = idx;

	return LIS_OK;
}


static enum lis_error fix_list_type(enum lis_value_type type, struct lis_value_list *list)
{
	int i;
	switch(type) {
		case LIS_TYPE_INTEGER:
			lis_log_debug(
				"Constraint of option '" OPT_NAME_RESOLUTION "'"
				" is already a list of integers"
			);
			return LIS_OK;
		case LIS_TYPE_DOUBLE:
			for (i = 0 ; i < list->nb_values ; i++) {
				lis_log_debug(
					"Converting resolution constraint list to integers: %f",
					list->values[i].dbl
				);
				list->values[i].integer = (int)list->values[i].dbl;
			}
			return LIS_OK;
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
	}
	lis_log_error("Unexpected value types for option '" OPT_NAME_RESOLUTION "' (list): %d", type);
	return LIS_ERR_UNSUPPORTED;
}


static enum lis_error opt_desc_filter(
		struct lis_item *item, struct lis_option_descriptor *opt, void *user_data
	)
{
	struct lis_value_list list;
	enum lis_error err;

	LIS_UNUSED(user_data);

	if (strcasecmp(opt->name, OPT_NAME_RESOLUTION) != 0) {
		return LIS_OK;
	}

	if (opt->value.type == LIS_TYPE_INTEGER && opt->constraint.type == LIS_CONSTRAINT_LIST) {
		lis_log_info("No change to do on option '" OPT_NAME_RESOLUTION "'");
		return LIS_OK;
	}

	if (opt->constraint.type == LIS_CONSTRAINT_NONE) {
		if (opt->value.type != LIS_TYPE_INTEGER) {
			lis_log_error(
				"Don't know how to fix constraint of option '" OPT_NAME_RESOLUTION "':"
				" no constraint available and cannot set default constraint list"
				" because value type is not the one expected: %d != %d",
				opt->value.type, LIS_TYPE_INTEGER
			);
			return LIS_ERR_UNSUPPORTED;
		}
		lis_log_warning(
			"Don't know how to fix constraint of option '" OPT_NAME_RESOLUTION "':"
			" no constraint available. Will set DEFAULT CONSTRAINT LIST."
		);
		opt->constraint.type = LIS_CONSTRAINT_LIST;
		opt->constraint.possible.list.nb_values = LIS_COUNT_OF(DEFAULT_CONSTRAINT);
		opt->constraint.possible.list.values = (union lis_value *)DEFAULT_CONSTRAINT;
		return LIS_OK;
	}

	free(lis_bw_item_get_user_ptr(item));
	lis_bw_item_set_user_ptr(item, NULL);

	if (opt->value.type != LIS_TYPE_INTEGER) {
		opt->fn.set_value = opt_set_value;
		opt->fn.get_value = opt_get_value;
	}

	if (opt->constraint.type == LIS_CONSTRAINT_RANGE) {
		lis_log_info(
			"Option '" OPT_NAME_RESOLUTION "': Converting constraint range into"
			" constraint list"
		);
		err = fix_range_type(opt->value.type, &opt->constraint.possible.range);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
		opt->value.type = LIS_TYPE_INTEGER;

		err = range_to_list(&opt->constraint.possible.range, &list);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
		memcpy(&opt->constraint.possible.list, &list, sizeof(opt->constraint.possible.list));
		opt->constraint.type = LIS_CONSTRAINT_LIST;
		lis_bw_item_set_user_ptr(item, list.values);

	} else {
		lis_log_info(
			"Option '" OPT_NAME_RESOLUTION "': Converting double constraint list into"
			" integer constraint list"
		);

		err = fix_list_type(opt->value.type, &opt->constraint.possible.list);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
		opt->value.type = LIS_TYPE_INTEGER;

	}

	return LIS_OK;
}


static void on_close_item(struct lis_item *item, int root, void *user_data)
{
	LIS_UNUSED(root);
	LIS_UNUSED(user_data);
	lis_log_debug("Freeing data from item '%s'", item->name);
	free(lis_bw_item_get_user_ptr(item));
	lis_bw_item_set_user_ptr(item, NULL);
}


enum lis_error lis_api_normalizer_resolution(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
	lis_bw_set_on_close_item(*impl, on_close_item, NULL);
	return err;
}
