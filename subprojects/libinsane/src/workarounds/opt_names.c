#include <string.h>

#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "../basewrapper.h"


#define NAME "workaround_opt_names"


struct opt_name_mapping {
	const char *original;
	const char *replacement;
};


static const struct opt_name_mapping g_opt_name_mapping[] = {
	{ .original = "scan-resolution", .replacement = OPT_NAME_RESOLUTION }, // Sane + Lexmark
	{ .original = "doc-source", .replacement = OPT_NAME_SOURCE }, // Sane + Samsung
};


static int get_mapping(const char *original_opt_name)
{
	unsigned int mapping_idx;
	for (mapping_idx = 0 ; mapping_idx < LIS_COUNT_OF(g_opt_name_mapping) ; mapping_idx++) {
		if (strcasecmp(g_opt_name_mapping[mapping_idx].original, original_opt_name) == 0) {
			return mapping_idx;
		}
	}
	return -1;
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	int *enabled_mappings;
	struct lis_item *original_item;
	struct lis_option_descriptor **opt_descs;
	int opt_idx;
	unsigned int mapping_idx;
	enum lis_error err;

	LIS_UNUSED(root);
	LIS_UNUSED(user_data);

	enabled_mappings = lis_bw_item_get_user_ptr(item);
	FREE(enabled_mappings);
	lis_bw_item_set_user_ptr(item, NULL);

	original_item = lis_bw_get_original_item(item);
	err = original_item->get_options(original_item, &opt_descs);

	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
				"Failed to get options: %d, %s. Will disabled aliases",
				err, lis_strerror(err)
				);
		return LIS_OK;
	}

	// we only enable mapping if the replacement option name isn't used
	// yet --> we check if the replacement names are already used or not.

	enabled_mappings = calloc(LIS_COUNT_OF(g_opt_name_mapping), sizeof(int));
	if (enabled_mappings == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	lis_bw_item_set_user_ptr(item, enabled_mappings);

	for (mapping_idx = 0 ; mapping_idx < LIS_COUNT_OF(g_opt_name_mapping) ; mapping_idx ++) {
		for (opt_idx = 0 ; opt_descs[opt_idx] != NULL ; opt_idx++) {
			if (strcasecmp(
						opt_descs[opt_idx]->name,
						g_opt_name_mapping[mapping_idx].replacement
					) == 0) {
				break;
			}
		}

		if (opt_descs[opt_idx] == NULL) {
			enabled_mappings[mapping_idx] = 1;
		}
	}

	return LIS_OK;
}


static enum lis_error opt_desc_filter(
		struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
	)
{
	int mapping_idx;
	int *enabled_mappings;
	const struct opt_name_mapping *mapping;

	LIS_UNUSED(user_data);

	mapping_idx = get_mapping(desc->name);
	if (mapping_idx < 0) {
		return LIS_OK;
	}
	mapping = &g_opt_name_mapping[mapping_idx];

	enabled_mappings = lis_bw_item_get_user_ptr(item);

	if (enabled_mappings == NULL || !enabled_mappings[mapping_idx]) {
		lis_log_warning(
			"Found option '%s' but option '%s' already exists too, so we can't rename it.",
			desc->name, mapping->replacement
		);
		return LIS_OK;
	}

	lis_log_debug("Renaming option '%s' into '%s'", desc->name, mapping->replacement);
	desc->name = mapping->replacement;
	return LIS_OK;
}


static void on_close_item(struct lis_item *item, int root, void *user_data)
{
	int *enabled_mappings;

	LIS_UNUSED(user_data);
	LIS_UNUSED(root);

	enabled_mappings = lis_bw_item_get_user_ptr(item);
	lis_bw_item_set_user_ptr(item, NULL);
	FREE(enabled_mappings);
}


enum lis_error lis_api_workaround_opt_names(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_item_filter(*impl, item_filter, NULL);
		lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
		lis_bw_set_on_close_item(*impl, on_close_item, NULL);
	}
	return err;

}
