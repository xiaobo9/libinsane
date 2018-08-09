#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include <libinsane/constants.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "normalizer_source_names"

#define REPLACE_STR "#"

static struct {
	const char *regex;
	const char *replacement;
	int lowercase;
	int compiled;
	regex_t preg;
} g_source_name_mappings[] = {
	{
		// unchanged
		.regex = "^" OPT_VALUE_SOURCE_FLATBED "(.*)",
		.replacement = OPT_VALUE_SOURCE_FLATBED REPLACE_STR,
		.lowercase = 0,
		.compiled = 0,
	},
	{
		// unchanged
		.regex = "^" OPT_VALUE_SOURCE_ADF "(.*)",
		.replacement = OPT_VALUE_SOURCE_ADF REPLACE_STR,
		.lowercase = 0,
		.compiled = 0,
	},
	{
		// Sane
		.regex = "^adf(.*)",
		.replacement = OPT_VALUE_SOURCE_ADF REPLACE_STR,
		.lowercase = 0,
		.compiled = 0,
	},
	{
		// Sane
		.regex = "^automatic document feeder(.*)",
		.replacement = OPT_VALUE_SOURCE_ADF REPLACE_STR,
		.lowercase = 0,
		.compiled = 0,
	},
	{
		// Sane + Epson Perfection v19
		.regex = "^document table(.*)",
		.replacement = OPT_VALUE_SOURCE_FLATBED REPLACE_STR,
		.lowercase = 0,
		.compiled = 0,
	},
	{
		// WIA
		.regex = "^[0-9]+\\\\Root\\\\(.*)",
		.replacement = REPLACE_STR,
		.lowercase = 1,
		.compiled = 0,
	},
	{
		.regex = NULL
	},
};

static int g_refcount = 0;


static char *replace_str(
		const char *original, const char *to_replace,
		const char *replacement, size_t replacement_len,
		int lowercase
	)
{
	const char *rptr;
	size_t new_size;
	char *out;
	size_t i;
	int c_idx;

	rptr = strstr(original, to_replace);
	if (to_replace == NULL) {
		lis_log_error("Internal error: invalid replacement");
		return NULL;
	}

	new_size = strlen(original) - strlen(to_replace) + replacement_len;
	out = calloc(new_size + 1, sizeof(char));
	if (out == NULL) {
		lis_log_error("Out of memory");
		return NULL;
	}

	snprintf(out, new_size + 1, "%.*s%.*s%s",
		(int)(rptr - original), original,
		(int)(replacement_len), replacement,
		rptr + strlen(REPLACE_STR));

	if (lowercase) {
		for (i = 0 ; i < replacement_len ; i++) {
			c_idx = i + ((int)(rptr - original));
			out[c_idx] = (char)tolower(out[c_idx]);
		}
	}

	out[new_size] = '\0';
	return out;
}


static enum lis_error compile_regexes(void)
{
	int i, r;

	lis_log_debug("Compiling regexes ...");
	for (i = 0 ; g_source_name_mappings[i].regex != NULL ; i++) {
		r = regcomp(
			&g_source_name_mappings[i].preg,
			g_source_name_mappings[i].regex,
			REG_ICASE | REG_EXTENDED
		);
		if (r != 0) {
			char buf[256];
			regerror(r, &g_source_name_mappings[i].preg, buf, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			lis_log_error("Failed to compile regex: [%s]: %d, %s",
				g_source_name_mappings[i].regex,
				r, buf
			);
			return (r == REG_ESPACE ? LIS_ERR_NO_MEM : LIS_ERR_INTERNAL_UNKNOWN_ERROR);
		}
		g_source_name_mappings[i].compiled = 1;
	}
	lis_log_debug("Regexes compiled successfully");
	return LIS_OK;
}


static void free_regexes(void)
{
	int i;

	lis_log_debug("Freeing regexes");
	for (i = 0 ; g_source_name_mappings[i].regex != NULL ; i++) {
		if (g_source_name_mappings[i].compiled) {
			regfree(&g_source_name_mappings[i].preg);
			g_source_name_mappings[i].compiled = 0;
		}
	}
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	int i, r;
	char *modified;
	regmatch_t matches[2];

	LIS_UNUSED(user_data);

	if (root) {
		lis_log_debug("Source '%s': root -> no name normalization", item->name);
		return LIS_OK;
	}

	for (i = 0 ; g_source_name_mappings[i].regex != NULL ; i++) {
		assert(g_source_name_mappings[i].compiled);
		memset(&matches, 0, sizeof(matches));
		r = regexec(
			&g_source_name_mappings[i].preg, item->name,
			LIS_COUNT_OF(matches), matches, 0 /* eflags */
		);
		if (r == REG_NOMATCH) {
			continue;
		}
		if (r != 0) {
			lis_log_error("Regex %d has failed, code=%d !", i, r);
			return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
		}
		modified = replace_str(
			g_source_name_mappings[i].replacement, REPLACE_STR,
			item->name + matches[1].rm_so,
			matches[1].rm_eo - matches[1].rm_so,
			g_source_name_mappings[i].lowercase
		);
		lis_log_info("%s -> %s -> %.*s (%d-%d) -> %s",
			item->name, g_source_name_mappings[i].regex,
			matches[1].rm_eo - matches[1].rm_so, item->name + matches[1].rm_so,
			matches[1].rm_eo, matches[1].rm_so,
			modified
		);
		item->name = modified;
		lis_bw_item_set_user_ptr(item, modified);
		return LIS_OK;
	}

	lis_log_warning("Failed to normalize name of source '%s'", item->name);
	return LIS_OK;
}


static void clean_impl(struct lis_api *impl, void *user_data)
{
	LIS_UNUSED(impl);
	LIS_UNUSED(user_data);

	g_refcount--;
	assert(g_refcount >= 0);
	if (g_refcount <= 0) {
		free_regexes();
	}
}


static void item_close(struct lis_item *item, int root, void *user_data)
{
	LIS_UNUSED(user_data);
	LIS_UNUSED(root);
	free(lis_bw_item_get_user_ptr(item));
}


enum lis_error lis_api_normalizer_source_names(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	if (g_refcount <= 0) {
		err = compile_regexes();
		if (LIS_IS_ERROR(err)) {
			free_regexes();
			return err;
		}
	}
	g_refcount++;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	lis_bw_set_item_filter(*impl, item_filter, NULL);
	lis_bw_set_on_close_item(*impl, item_close, NULL);
	lis_bw_set_clean_impl(*impl, clean_impl, NULL);
	return err;
}
