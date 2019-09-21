#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <libinsane/constants.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"


#define NAME "normalizers_safe_defaults"

/*!
 * \brief set the default value immediately.
 * Set the default value as soon as the item is requested.
 * Note that it should be done for all values anyway, so the application
 * can see the actual default values when requesting options.
 */
#define SET_IMMEDIATELY (1 << 0)

/*!
 * \brief set the default value right before scanning.
 * Set the default value when the application request a scan session.
 * This is recommended for options having constraints that may have changed
 * when other options were changed (for instance, depending on the units,
 * changing the resolution may change the scan area constraints).
 */
#define SET_BEFORE_SCAN (1 << 1)


struct safe_setter {
	const char *opt_name;
	enum lis_error (*cb)(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
	void *cb_data;
	int flags;
};


static enum lis_error set_to_limit(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_str(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_preview(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_int(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_boolean(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);

static int g_numbers[] = { -1, 1, 0, 300, 24 };

static const struct safe_setter g_safe_setters[] = {
	// all backends:
	{
		.opt_name = OPT_NAME_MODE, .cb = set_str,
		.cb_data = OPT_VALUE_MODE_COLOR, .flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = OPT_NAME_PREVIEW, .cb = set_preview,
		.cb_data = &g_numbers[2] /* false */, .flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = OPT_NAME_RESOLUTION, .cb = set_int,
		.cb_data = &g_numbers[3] /* 300 */, .flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = OPT_NAME_TL_X, .cb = set_to_limit,
		.cb_data = &g_numbers[0],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},
	{
		.opt_name = OPT_NAME_TL_Y, .cb = set_to_limit,
		.cb_data = &g_numbers[0],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},
	{
		.opt_name = OPT_NAME_BR_X, .cb = set_to_limit,
		.cb_data = &g_numbers[1],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},
	{
		.opt_name = OPT_NAME_BR_Y, .cb = set_to_limit,
		.cb_data = &g_numbers[1],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},

	// WORKAROUND(Jflesch): Fujistu SnapScan S1500 (Sane) + Fujistu SnapScan iX500 (Sane)
	// page-width: Specifies the width of the media. Required for automatic centering of sheet-fed scans.
	// page-height: Specifies the height of the media.
	// ==> Default values are crap.
	// ==> Since this feature is Fujistu-specific, here we disable automatic centering.
	{
		.opt_name = "page-width", .cb = set_to_limit,
		.cb_data = &g_numbers[1],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},
	{
		.opt_name = "page-height", .cb = set_to_limit,
		.cb_data = &g_numbers[1],
		.flags = SET_IMMEDIATELY | SET_BEFORE_SCAN,
	},

	// Sane test backend:
	{
		.opt_name = "test-picture", .cb = set_str,
		.cb_data = "Color pattern",
		.flags = SET_IMMEDIATELY,
	},

	// WIA2:
	{
		. opt_name = "pages", .cb = set_int,
		.cb_data = &g_numbers[2], /* 0 = infinite */
		.flags = SET_IMMEDIATELY,
	},

	// TWAIN:
	{
		.opt_name = "transfer_count", .cb = set_int,
		.cb_data = &g_numbers[0], /* -1 */
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "compression", .cb = set_str,
		.cb_data = "none",
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "transfer_mechanism", .cb = set_str,
		.cb_data = "native",
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "image_file_format", .cb = set_str,
		.cb_data = "bmp",
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "bit_depth", .cb = set_int,
		.cb_data = &g_numbers[4], /* 24 */
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "indicators", .cb = set_boolean,
		.cb_data = NULL, /* FALSE */
		.flags = SET_IMMEDIATELY,
	},
	{
		.opt_name = "supported_sizes", .cb = set_str,
		.cb_data = "none",
		.flags = SET_IMMEDIATELY,
	},
	{ .opt_name = NULL },
};


static enum lis_error set_to_limit(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	int minmax = *((int *)cb_data);
	union lis_value value;
	enum lis_error err;
	const char *minmax_str = minmax > 0 ? "max" : "min";

	if (opt->constraint.type != LIS_CONSTRAINT_RANGE) {
		lis_log_warning("Unexpected constraint type for option '%s': %d instead of %d",
			opt->name, opt->constraint.type, LIS_CONSTRAINT_RANGE);
		return LIS_ERR_UNSUPPORTED;
	}

	if (opt->value.type != LIS_TYPE_INTEGER && opt->value.type != LIS_TYPE_DOUBLE) {
		lis_log_warning(
			"Unexpected value type for option '%s': %d",
			opt->name, opt->value.type
		);
		return LIS_ERR_UNSUPPORTED;
	}

	// check the current value to see if it's currently in the range or
	// not
	err = opt->fn.get_value(opt, &value);
	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
			"Failed to get current value of '%s': %d, %s",
			opt->name, err, lis_strerror(err)
		);
		value = (
			minmax > 0
			? opt->constraint.possible.range.max
			: opt->constraint.possible.range.min
		);
	} else {
		lis_log_info("Current value of option '%s' = %d", opt->name, value.integer);
		// Sane + Epson Perfection 1250
		// https://gitlab.gnome.org/World/OpenPaperwork/libinsane/issues/17
		// https://openpaper.work/en-us/scanner_db/report/328/
		if (opt->value.type == LIS_TYPE_INTEGER) {
			if (minmax > 0) {
				// if the current value is already above the max, we keep it as it
				value.integer = MAX(value.integer, opt->constraint.possible.range.max.integer);
			} else {
				// if the current value is already below the min, we keep it as it
				value.integer = MIN(value.integer, opt->constraint.possible.range.min.integer);
			}
		} else if (opt->value.type == LIS_TYPE_DOUBLE) {
			if (minmax > 0) {
				// if the current value is already above the max, we keep it as it
				value.dbl = MAX(value.dbl, opt->constraint.possible.range.max.dbl);
			} else {
				// if the current value is already below the min, we keep it as it
				value.dbl = MIN(value.dbl, opt->constraint.possible.range.min.dbl);
			}
		} else {
			assert(0);
		}
	}

	lis_log_info("Setting option '%s' to %s", opt->name, minmax_str);
	err = opt->fn.set_value(opt, value, set_flags);
	if (LIS_IS_OK(err)) {
		lis_log_info("'%s'=%s: 0x%X, %s (set_flags=0x%X)",
			opt->name, minmax_str, err, lis_strerror(err), *set_flags);
	} else {
		*set_flags = 0;
		lis_log_warning("'%s'=%s: 0x%X, %s",
			opt->name, minmax_str, err, lis_strerror(err));
	}
	return err;
}


static enum lis_error set_str(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	union lis_value value;
	enum lis_error err;

	value.string = cb_data;
	lis_log_info("Setting option '%s' to '%s'", opt->name, value.string);

	if (opt->value.type != LIS_TYPE_STRING) {
		lis_log_warning("Cannot set option '%s' to '%s': Option doesn't accept string as value (%d)",
			opt->name, value.string, opt->value.type);
		return LIS_ERR_UNSUPPORTED;
	}

	err = opt->fn.set_value(opt, value, set_flags);
	if (LIS_IS_OK(err)) {
		lis_log_info("'%s'='%s': 0x%X, %s (set_flags=0x%X)",
			opt->name, value.string, err, lis_strerror(err), *set_flags);
	} else {
		*set_flags = 0;
		lis_log_warning("'%s'='%s': 0x%X, %s",
			opt->name, value.string, err, lis_strerror(err));
	}
	return err;
}


static enum lis_error set_boolean(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	union lis_value value;
	enum lis_error err;

	value.boolean = (cb_data != NULL);
	lis_log_info("Setting option '%s' to '%d'", opt->name, value.boolean);

	if (opt->value.type == LIS_TYPE_BOOL) { // Sane test backend

		err = opt->fn.set_value(opt, value, set_flags);
		if (LIS_IS_OK(err)) {
			lis_log_info("'%s'='%d': 0x%X, %s (set_flags=0x%X)",
				opt->name, value.boolean, err, lis_strerror(err), *set_flags);
		} else {
			*set_flags = 0;
			lis_log_warning("'%s'='%d': 0x%X, %s",
				opt->name, value.boolean, err, lis_strerror(err));
		}
		return err;

	} else {
		lis_log_warning("Cannot set option '%s' to '%d': Option doesn't accept boolean as value (%d)",
		opt->name, value.boolean, opt->value.type);
		return LIS_ERR_UNSUPPORTED;
	}

}


static enum lis_error set_preview(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	union lis_value value;
	enum lis_error err;

	value.boolean = *((int *)cb_data);
	lis_log_info("Setting option '%s' to '%d'", opt->name, value.boolean);

	if (opt->value.type == LIS_TYPE_BOOL) { // Sane test backend

		err = opt->fn.set_value(opt, value, set_flags);
		if (LIS_IS_OK(err)) {
			lis_log_info("'%s'='%d': 0x%X, %s (set_flags=0x%X)",
				opt->name, value.boolean, err, lis_strerror(err), *set_flags);
		} else {
			*set_flags = 0;
			lis_log_warning("'%s'='%d': 0x%X, %s",
				opt->name, value.boolean, err, lis_strerror(err));
		}
		return err;

	} else if (opt->value.type == LIS_TYPE_STRING) { // WIA2

		value.string = "final";

		err = opt->fn.set_value(opt, value, set_flags);
		if (LIS_IS_OK(err)) {
			lis_log_info("'%s'='%d': 0x%X, %s (set_flags=0x%X)",
				opt->name, value.boolean, err, lis_strerror(err), *set_flags);
		} else {
			*set_flags = 0;
			lis_log_warning("'%s'='%d': 0x%X, %s",
				opt->name, value.boolean, err, lis_strerror(err));
		}
		return err;

	} else {
		lis_log_warning("Cannot set option '%s' to '%d': Option doesn't accept boolean as value (%d)",
		opt->name, value.boolean, opt->value.type);
		return LIS_ERR_UNSUPPORTED;
	}
}

static enum lis_error set_int(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	union lis_value value;
	enum lis_error err;
	int closest, closest_distance, distance, constraint_idx;

	value.integer = *((int *)cb_data);
	lis_log_info("Setting option '%s' (%d) to '%d'", opt->name, opt->value.type, value.integer);

	if (opt->value.type != LIS_TYPE_INTEGER) {
		lis_log_warning("Cannot set option '%s' to '%d': Option doesn't accept integer as value (%d)",
			opt->name, value.integer, opt->value.type);
		return LIS_ERR_UNSUPPORTED;
	}

	if (opt->constraint.type != LIS_CONSTRAINT_LIST || opt->constraint.possible.list.nb_values <= 0) {
		lis_log_warning("Unexpected constraint type (%d) for option '%s'. Cannot adjust value.",
			opt->constraint.type, opt->name);
	} else {
		closest = 0;
		closest_distance = 999999;
		for (constraint_idx = 0 ;
				constraint_idx < opt->constraint.possible.list.nb_values ;
				constraint_idx++) {

			distance = abs(
				opt->constraint.possible.list.values[constraint_idx].integer
				- value.integer
			);
			if (distance < closest_distance) {
				closest = opt->constraint.possible.list.values[constraint_idx].integer;
				closest_distance = distance;
			}
		}
		if (closest != value.integer) {
			lis_log_info("Value for option '%s' adjusted to match constraint: %d => %d",
				opt->name, value.integer, closest);
			value.integer = closest;
		}
	}

	err = opt->fn.set_value(opt, value, set_flags);
	if (LIS_IS_OK(err)) {
		lis_log_info("'%s'='%d': 0x%X, %s (set_flags=0x%X)",
			opt->name, value.integer, err, lis_strerror(err), *set_flags);
	} else {
		*set_flags = 0;
		lis_log_warning("'%s'='%d': 0x%X, %s",
			opt->name, value.integer, err, lis_strerror(err));
	}
	return err;
}


static enum lis_error set_default_values(
		struct lis_item *item, int required_flag
	)
{
	struct lis_option_descriptor **opts = NULL;
	struct lis_option_descriptor **opt;
	struct lis_option_descriptor *original;
	const struct safe_setter *setter;
	int set_flags;
	enum lis_error err;

	set_flags = LIS_SET_FLAG_MUST_RELOAD_OPTIONS;

	for (setter = g_safe_setters; setter->opt_name != NULL ; setter++) {
		if (!(setter->flags & required_flag)) {
			continue;
		}
		lis_log_info(
			NAME ": set_default_value(%s) ...",
			setter->opt_name
		);

		if (set_flags & LIS_SET_FLAG_MUST_RELOAD_OPTIONS) {
			lis_log_debug(
				NAME ": set_default_value(%s): Must first:"
				" Reload option list of item '%s'...",
				setter->opt_name, item->name
			);
			err = item->get_options(item, &opts);
			if (LIS_IS_ERROR(err)) {
				lis_log_error(
					NAME ": set_default_value(%s):"
					" Failed to get "
					" options of item '%s': 0x%X, %s",
					setter->opt_name, item->name,
					err, lis_strerror(err)
				);
				return err;
			}
			set_flags = 0;
		}

		for (opt = opts ; (*opt) != NULL ; opt++) {
			if (strcasecmp(setter->opt_name, (*opt)->name) == 0) {
				break;
			}
		}
		if ((*opt) == NULL) {
			lis_log_info(
				NAME ": set_default_value(%s): Option not"
				" found", setter->opt_name
			);
			continue;
		}

		if (lis_bw_opt_get_user_ptr(*opt) != NULL) {
			lis_log_info(
				NAME ": set_default_value(%s): Option already"
				" set by user app. Won't set it to default"
				" value.",
				setter->opt_name
			);
			continue;
		}

		original = lis_bw_get_original_opt(*opt);
		set_flags = 0;
		err = setter->cb(original, setter->cb_data, &set_flags);
		if (LIS_IS_OK(err)) {
			lis_log_info(
				NAME ": set_default_value(%s): OK",
				setter->opt_name
			);
		} else {
			lis_log_warning(
				NAME ":set default_value(%s):"
				" Failed to set option"
				" to safe default: 0x%X, %s",
				(*opt)->name,
				err, lis_strerror(err)
			);
			// still worth trying scanning
		}
	}

	return LIS_OK;
}


static enum lis_error scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	enum lis_error err;
	struct lis_item *original = lis_bw_get_original_item(self);

	lis_log_info("Scan start requested. Setting some late default values");
	err = set_default_values(self, SET_BEFORE_SCAN);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	return original->scan_start(original, session);
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	enum lis_error err;

	LIS_UNUSED(user_data);
	LIS_UNUSED(root);

	item->scan_start = scan_start;
	lis_log_info("Setting default values on item '%s'", item->name);
	err = set_default_values(item, SET_IMMEDIATELY);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	return LIS_OK;
}


static enum lis_error opt_set_value(
		struct lis_option_descriptor *self,
		union lis_value value, int *set_flags
	)
{
	enum lis_error err;
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);

	err = original->fn.set_value(original, value, set_flags);

	if (LIS_IS_OK(err)) {
		// put a dummy non-NULL user pointer to mark the option as set
		lis_bw_opt_set_user_ptr(self, (void*)0xDEADBEEF, NULL);
	}
	return err;
}


static enum lis_error opt_desc_filter(
		struct lis_item *item, struct lis_option_descriptor *desc,
		void *user_data
	)
{
	LIS_UNUSED(item);
	LIS_UNUSED(user_data);

	desc->fn.set_value = opt_set_value;
	return LIS_OK;
}


enum lis_error lis_api_normalizer_safe_defaults(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_item_filter(*impl, item_filter, NULL);
		lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
	}
	return err;
}
