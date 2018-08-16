#include <assert.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/workarounds.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "workaround_opt_values"


struct opt_value_mapping {
	const char *original;
	const char *replacement;
};

struct opt_values_mapping {
	const char *opt_name;
	const struct opt_value_mapping *mapping;
};

const struct opt_value_mapping g_opt_mode_mapping[] = {
	// Sane + Brother MFC-7360N:
	// 'Black & White', 'Gray[Error Diffusion]', 'True Gray', '24bit Color', '24bit Color[Fast]'
	{ .original = "Black & White", .replacement = OPT_VALUE_MODE_BW },
	{ .original = "True Gray", .replacement = OPT_VALUE_MODE_GRAYSCALE },
	{ .original = "24bit Color", .replacement = OPT_VALUE_MODE_COLOR },

	// Sane + OKI MC363: translations ...
	{ .original = "Couleur", .replacement = OPT_VALUE_MODE_COLOR },
	{ .original = "Gris", .replacement = OPT_VALUE_MODE_GRAYSCALE },
	{ .original = "Noir et blanc", .replacement = OPT_VALUE_MODE_BW },

	{ .original = NULL, .replacement = NULL },
};


const struct opt_value_mapping g_opt_source_mapping[] = {
	// Sane + OKI MC363: translations ...
	{
		 .original = (char *) (unsigned char[]) {
			 'S', 'c', 'a', 'n', 'n', 'e', 'u', 'r',
			 ' ', 0xe0,
			 ' ', 'p', 'l', 'a', 't', '\0'
		},
		 .replacement = OPT_VALUE_SOURCE_FLATBED
	},
	{
		 .original = (char *) (unsigned char[]) {
			'S', 'c', 'a', 'n', 'n', 'e', 'u', 'r',
			' ', 0xC3, 0xA0,
			' ', 'p', 'l', 'a', 't', '\0'
		 },
		 .replacement = OPT_VALUE_SOURCE_FLATBED
	},
	{ .original = "Chargeur automatique de documents (ADF)", .replacement = OPT_VALUE_SOURCE_ADF },

	{ .original = NULL, .replacement = NULL },
};


const struct opt_values_mapping g_opt_values_mapping[] = {
	{ .opt_name = OPT_NAME_MODE, .mapping = g_opt_mode_mapping },
	{ .opt_name = OPT_NAME_SOURCE, .mapping = g_opt_source_mapping },
	{ .opt_name = NULL, },
};


const struct opt_values_mapping *get_opt_mapping(const char *opt_name)
{
	int mapping_idx;
	for (mapping_idx = 0 ; g_opt_values_mapping[mapping_idx].opt_name != NULL ; mapping_idx++) {
		if (strcasecmp(g_opt_values_mapping[mapping_idx].opt_name, opt_name) == 0) {
			lis_log_debug("Mapping found for option '%s'", opt_name);
			return &g_opt_values_mapping[mapping_idx];
		}
	}
	lis_log_debug("No mapping for option '%s'", opt_name);
	return NULL;
}


const struct opt_value_mapping *get_opt_original_value_mapping(const char *opt_name, const char *opt_value)
{
	const struct opt_values_mapping *o_mapping;
	const struct opt_value_mapping *v_mapping;

	o_mapping = get_opt_mapping(opt_name);
	if (o_mapping == NULL) {
		return NULL;
	}

	for (v_mapping = o_mapping->mapping ; v_mapping->original != NULL ; v_mapping++) {
		if (strcasecmp(v_mapping->original, opt_value) == 0) {
			lis_log_debug("Mapping found for option '%s' + value '%s'",
				opt_name, opt_value);
			return v_mapping;
		}
	}
	lis_log_debug("No mapping found for option '%s' + value '%s'",
			opt_name, opt_value);
	return NULL;
}


const struct opt_value_mapping *get_opt_modified_value_mapping(
		const char *opt_name, const char *opt_value, const struct lis_value_list *constraint
	)
{
	const struct opt_values_mapping *o_mapping;
	const struct opt_value_mapping *v_mapping;
	int constraint_idx;

	o_mapping = get_opt_mapping(opt_name);
	if (o_mapping == NULL) {
		return NULL;
	}

	for (v_mapping = o_mapping->mapping ; v_mapping->original != NULL ; v_mapping++) {
		if (strcasecmp(opt_value, v_mapping->replacement) != 0) {
			continue;
		}
		for (constraint_idx = 0 ; constraint_idx < constraint->nb_values ; constraint_idx++) {
			if (strcasecmp(v_mapping->original, constraint->values[constraint_idx].string) == 0) {
				lis_log_debug("Mapping found for option '%s' + value '%s' (%s)",
					opt_name, opt_value, v_mapping->original);
				return v_mapping;
			}
		}
	}
	lis_log_debug("No mapping found for option '%s' + value '%s'",
			opt_name, opt_value);
	return NULL;
}


enum lis_error get_value(struct lis_option_descriptor *modified, union lis_value *value)
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

	mapping = get_opt_original_value_mapping(modified->name, value->string);
	if (mapping == NULL) {
		return err;
	}

	value->string = mapping->replacement;
	return err;
}


enum lis_error set_value(struct lis_option_descriptor *modified,
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

	mapping = get_opt_modified_value_mapping(modified->name, value.string,
		&original->constraint.possible.list);
	if (mapping != NULL) {
		value.string = mapping->original;
	}

	return original->fn.set_value(original, value, set_flags);
}


static enum lis_error opt_desc_filter(
		struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
	)
{
	int constraint_idx;
	const struct opt_value_mapping *mapping;

	LIS_UNUSED(item);
	LIS_UNUSED(user_data);

	if (desc->value.type != LIS_TYPE_STRING || desc->constraint.type != LIS_CONSTRAINT_LIST) {
		return LIS_OK;
	}

	for (constraint_idx = 0 ;
			constraint_idx < desc->constraint.possible.list.nb_values ;
			constraint_idx++) {
		mapping = get_opt_original_value_mapping(
			desc->name,
			desc->constraint.possible.list.values[constraint_idx].string
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
		desc->fn.get_value = get_value;
		desc->fn.set_value = set_value;
	}

	return LIS_OK;
}


enum lis_error lis_api_workaround_opt_values(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
	}
	return err;
}
