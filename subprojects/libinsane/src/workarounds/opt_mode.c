#include <assert.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/workarounds.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "workaround_opt_mode"


struct opt_value_mapping {
	const char *original;
	const char *replacement;
};

const struct opt_value_mapping g_opt_value_mapping[] = {
	// Sane + Brother MFC-7360N:
	// 'Black & White', 'Gray[Error Diffusion]', 'True Gray', '24bit Color', '24bit Color[Fast]'
	{ .original = "Black & White", .replacement = OPT_VALUE_MODE_BW },
	{ .original = "True Gray", .replacement = OPT_VALUE_MODE_GRAYSCALE },
	{ .original = "24bit Color", .replacement = OPT_VALUE_MODE_COLOR },
	{ .original = NULL, .replacement = NULL },
};


const struct opt_value_mapping *get_mapping(const char *opt_value, int original) {
	int mapping_idx;
	const char *mapping_name;
	for (mapping_idx = 0 ; g_opt_value_mapping[mapping_idx].original != NULL ; mapping_idx++) {
		mapping_name = (original
				? g_opt_value_mapping[mapping_idx].original
				: g_opt_value_mapping[mapping_idx].replacement);
		if (strcasecmp(mapping_name, opt_value) == 0) {
			return &g_opt_value_mapping[mapping_idx];
		}
	}
	return NULL;
}


enum lis_error opt_mode_get_value(struct lis_option_descriptor *modified, union lis_value *value)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(modified);
	enum lis_error err;
	const struct opt_value_mapping *mapping;

	assert(modified->value.type == LIS_TYPE_STRING);
	assert(modified->constraint.type == LIS_CONSTRAINT_LIST);

	if (original == NULL) {
		lis_log_error("Can't find back option %s (%p) !", modified->name, (void *)modified);
		return LIS_ERR_INVALID_VALUE;
	}

	err = original->fn.get_value(original, value);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	mapping = get_mapping(value->string, 1 /* original */);
	if (mapping == NULL) {
		return err;
	}

	value->string = mapping->replacement;
	return err;
}


enum lis_error opt_mode_set_value(struct lis_option_descriptor *modified,
				union lis_value value, int *set_flags)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(modified);
	const struct opt_value_mapping *mapping;

	assert(modified->value.type == LIS_TYPE_STRING);
	assert(modified->constraint.type == LIS_CONSTRAINT_LIST);

	if (original == NULL) {
		lis_log_error("Can't find back option %s (%p) !", modified->name, (void *)modified);
		return LIS_ERR_INVALID_VALUE;
	}

	mapping = get_mapping(value.string, 0 /* replacement */);
	if (mapping != NULL) {
		value.string = mapping->original;
	}

	return original->fn.set_value(original, value, set_flags);
}


static enum lis_error opt_desc_filter(
		const struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
	)
{
	int constraint_idx;
	const struct opt_value_mapping *mapping;

	LIS_UNUSED(item);
	LIS_UNUSED(user_data);

	if (strcasecmp(desc->name, OPT_NAME_MODE) != 0) {
		lis_log_debug(NAME ": Option '%s' unchanged", desc->name);
		return LIS_OK;
	}

	if (desc->value.type != LIS_TYPE_STRING) {
		lis_log_error(NAME ": Unexpected value type for option '" OPT_NAME_MODE "': %d",
				desc->value.type);
		return LIS_ERR_UNSUPPORTED; /* shouldn't happen */
	}
	if (desc->constraint.type != LIS_CONSTRAINT_LIST) {
		lis_log_error(NAME ": Unexpected constraint type for option '" OPT_NAME_MODE "': %d",
				desc->constraint.type);
		return LIS_ERR_UNSUPPORTED; /* shouldn't happen */
	}

	for (constraint_idx = 0 ;
			constraint_idx < desc->constraint.possible.list.nb_values ;
			constraint_idx++) {
		mapping = get_mapping(
				desc->constraint.possible.list.values[constraint_idx].string,
				1 /* original */
		);
		if (mapping == NULL) {
			continue;
		}
		/* switch pointers */
		lis_log_debug(
			"Replacing mode value '%s' -> '%s'",
			desc->constraint.possible.list.values[constraint_idx].string,
			mapping->replacement
		);
		desc->constraint.possible.list.values[constraint_idx].string = mapping->replacement;
		/* set custom callbacks */
		desc->fn.get_value = opt_mode_get_value;
		desc->fn.set_value = opt_mode_set_value;
	}

	return LIS_OK;
}


enum lis_error lis_api_workaround_opt_mode(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
	}
	return err;
}
