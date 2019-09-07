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
		struct lis_item *item, struct lis_option_descriptor *desc, void *user_data
	)
{
	const struct opt_name_mapping *mapping;
	enum lis_error err;
	struct lis_option_descriptor **opt_descs;
	int opt_idx;
	struct lis_item *original_item;

	LIS_UNUSED(user_data);

	mapping = get_mapping(desc->name);
	if (mapping == NULL) {
		return LIS_OK;
	}

	original_item = lis_bw_get_original_item(item);
	err = original_item->get_options(original_item, &opt_descs);
	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
			"Failed to get options: %d, %s. Assuming alias option doesn't exist yet",
			err, lis_strerror(err)
		);
	} else {
		// XXX(Jflesch):
		// Canon PIXMA MX520 Series + Sane: provide both 'scan-resolution'
		// and 'resolution'. 'scan-resolution' can safely be ignored.
		for (opt_idx = 0 ; opt_descs[opt_idx] != NULL ; opt_idx++) {
			if (strcasecmp(opt_descs[opt_idx]->name, mapping->replacement) == 0) {
				lis_log_info(
					"Got both option '%s'and '%s'. Not making any alias",
					desc->name, mapping->replacement
				);
				return LIS_OK;
			}
		}
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
