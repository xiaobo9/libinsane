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


const struct opt_name_mapping g_opt_name_mapping[] = {
	{ .original = "scan-resolution", .replacement = OPT_NAME_RESOLUTION }, // Sane + Lexmark
	{ .original = "doc-source", .replacement = OPT_NAME_SOURCE }, // Sane + Samsung
	{ .original = NULL, .replacement = NULL },
};


static const struct opt_name_mapping *get_mapping(const char *original_opt_name)
{
	int mapping_idx;
	for (mapping_idx = 0 ; g_opt_name_mapping[mapping_idx].original != NULL ; mapping_idx++) {
		if (strcasecmp(g_opt_name_mapping[mapping_idx].original, original_opt_name) == 0) {
			return &g_opt_name_mapping[mapping_idx];
		}
	}
	return NULL;
}


static enum lis_error opt_desc_filter(
		const struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
	)
{
	const struct opt_name_mapping *mapping;

	LIS_UNUSED(item);
	LIS_UNUSED(user_data);

	mapping = get_mapping(desc->name);
	if (mapping == NULL) {
		return LIS_OK;
	}

	lis_log_debug("Renaming option '%s' into '%s'", desc->name, mapping->replacement);
	desc->name = mapping->replacement;
	return LIS_OK;
}

enum lis_error lis_api_workaround_opt_names(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_opt_desc_filter(*impl, opt_desc_filter, NULL);
	}
	return err;

}
