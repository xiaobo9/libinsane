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

// mode = Color
// max scan area
// set_opt(source, 'test-picture', 'Color pattern')


struct safe_setter {
	const char *opt_name;
	enum lis_error (*cb)(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
	void *cb_data;
};


static enum lis_error set_to_limit(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_str(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_preview(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);
static enum lis_error set_int(struct lis_option_descriptor *opt, void *cb_data, int *set_flags);

static int g_numbers[] = { -1, 1, 0, 300};

static const struct safe_setter g_safe_setters[] = {
	// all backends:
	{ .opt_name = OPT_NAME_MODE, .cb = set_str, .cb_data = OPT_VALUE_MODE_COLOR },
	{ .opt_name = OPT_NAME_PREVIEW, .cb = set_preview, .cb_data = &g_numbers[2] /* false */ },
	{ .opt_name = OPT_NAME_RESOLUTION, .cb = set_int, .cb_data = &g_numbers[3] /* 300 */ },
	{ .opt_name = OPT_NAME_TL_X, .cb = set_to_limit, .cb_data = &g_numbers[0] },
	{ .opt_name = OPT_NAME_TL_Y, .cb = set_to_limit, .cb_data = &g_numbers[0] },
	{ .opt_name = OPT_NAME_BR_X, .cb = set_to_limit, .cb_data = &g_numbers[1] },
	{ .opt_name = OPT_NAME_BR_Y, .cb = set_to_limit, .cb_data = &g_numbers[1] },

	// WORKAROUND(Jflesch): Fujistu SnapScan S1500 (Sane) + Fujistu SnapScan iX500 (Sane)
	// page-width: Specifies the width of the media. Required for automatic centering of sheet-fed scans.
	// page-height: Specifies the height of the media.
	// ==> Default values are crap.
	// ==> Since this feature is Fujistu-specific, here we disable automatic centering.
	{ .opt_name = "page-width", .cb = set_to_limit, .cb_data = &g_numbers[1] },
	{ .opt_name = "page-height", .cb = set_to_limit, .cb_data = &g_numbers[1] },

	// Sane test backend:
	{ .opt_name = "test-picture", .cb = set_str, .cb_data = "Color pattern" },

	// WIA2:
	{ . opt_name = "pages", .cb = set_int, .cb_data = &g_numbers[2] /* 0 = infinite */ },

	{ .opt_name = NULL },
};


static enum lis_error set_to_limit(struct lis_option_descriptor *opt, void *cb_data, int *set_flags)
{
	int minmax = *((int *)cb_data);
	union lis_value value;
	enum lis_error err;
	const char *minmax_str = minmax > 0 ? "max" : "min";

	value = minmax > 0 ? opt->constraint.possible.range.max : opt->constraint.possible.range.min;

	if (opt->constraint.type != LIS_CONSTRAINT_RANGE) {
		lis_log_warning("Unexpected constraint type for option '%s': %d instead of %d",
			opt->name, opt->constraint.type, LIS_CONSTRAINT_RANGE);
		return LIS_ERR_UNSUPPORTED;
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
	lis_log_info("Setting option '%s' to '%d'", opt->name, value.integer);

	if (opt->value.type != LIS_TYPE_INTEGER) {
		lis_log_warning("Cannot set option '%s' to '%d': Option doesn't accept boolean as value (%d)",
			opt->name, value.integer, opt->value.type);
		return LIS_ERR_UNSUPPORTED;
	}

	if (opt->constraint.type != LIS_CONSTRAINT_LIST || opt->constraint.possible.list.nb_values <= 0) {
		lis_log_warning("Unexpected constraint type (%d) for option '%s'. Cannot adjust value.",
			opt->constraint.type, opt->name);
	} else {
		closest = -1;
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
		assert(closest >= 0);
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


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	enum lis_error err;
	struct lis_option_descriptor **opts;
	int opt_idx;
	const struct safe_setter *setter;
	int set_flags;
	int setter_idx;

	LIS_UNUSED(user_data);

	for (setter_idx = 0; g_safe_setters[setter_idx].opt_name != NULL ; ) {

		lis_log_debug(NAME "->item_filter(): Getting options for item '%s' (%d)...", item->name, root);
		err = item->get_options(item, &opts);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(NAME "->item_filter(): Failed to get options of item '%s' (%d): 0x%X, %s",
				item->name, root, err, lis_strerror(err));
			return err;
		}

		for (; g_safe_setters[setter_idx].opt_name != NULL ; setter_idx++) {
			set_flags = 0;
			setter = &g_safe_setters[setter_idx];

			for (opt_idx = 0 ; opts[opt_idx] != NULL ; opt_idx++) {
				if (strcasecmp(setter->opt_name, opts[opt_idx]->name) != 0) {
					continue;
				}

				err = setter->cb(opts[opt_idx], setter->cb_data, &set_flags);
				if (LIS_IS_ERROR(err)) {
					lis_log_warning(
						NAME "->item_filter(): Failed to set option '%s'"
						" to safe default: 0x%X, %s",
						opts[opt_idx]->name, err, lis_strerror(err)
					);
					// still worth trying scanning
				}
				break;
			}
			if (set_flags & LIS_SET_FLAG_MUST_RELOAD_OPTIONS) {
				lis_log_info("Must reload options");
				setter_idx++;
				break;
			}
		}

	}

	return LIS_OK;
}


enum lis_error lis_api_normalizer_safe_defaults(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_item_filter(*impl, item_filter, NULL);
	}
	return err;
}
