#include <assert.h>
#include <sys/types.h>
#include <regex.h>

#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "normalizer_source_types"


struct {
	const char *regex;
	enum lis_item_type type;
	int compiled;
	regex_t preg;
} g_source_type_mappings[] = {
	{ .regex = "flatbed", .type = LIS_ITEM_FLATBED, .compiled = 0, },
	{ .regex = ".*Automatic Document Feeder.*", .type = LIS_ITEM_ADF, .compiled = 0 },
	{ .regex = ".*adf.*", .type = LIS_ITEM_ADF, .compiled = 0 },
	{ .regex = NULL },
};

static int g_refcount = 0;


static enum lis_error compile_regexes(void)
{
	int i, r;

	lis_log_debug("Compiling regexes ...");
	for (i = 0 ; g_source_type_mappings[i].regex != NULL ; i++) {
		r = regcomp(
			&g_source_type_mappings[i].preg,
			g_source_type_mappings[i].regex,
			REG_ICASE | REG_NOSUB
		);
		if (r != 0) {
			char buf[256];
			regerror(r, &g_source_type_mappings[i].preg, buf, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			lis_log_error("Failed to compile regex: [%s]: %d, %s",
				g_source_type_mappings[i].regex,
				r, buf
			);
			return (r == REG_ESPACE ? LIS_ERR_NO_MEM : LIS_ERR_INTERNAL_UNKNOWN_ERROR);
		}
		g_source_type_mappings[i].compiled = 1;
	}
	lis_log_debug("Regexes compiled successfully");
	return LIS_OK;
}


static void free_regexes(void)
{
	int i;

	lis_log_debug("Freeing regexes");
	for (i = 0 ; g_source_type_mappings[i].regex != NULL ; i++) {
		if (g_source_type_mappings[i].compiled) {
			regfree(&g_source_type_mappings[i].preg);
			g_source_type_mappings[i].compiled = 0;
		}
	}
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	int i, r;

	LIS_UNUSED(user_data);

	if (item->type != LIS_ITEM_UNIDENTIFIED) {
		lis_log_debug("Item '%s' has already a type: %d'", item->name, item->type);
		return LIS_OK;
	}

	if (root) {
		lis_log_debug("Item '%s': root -> type = LIS_ITEM_DEVICE", item->name);
		item->type = LIS_ITEM_DEVICE;
		return LIS_OK;
	}

	for (i = 0 ; g_source_type_mappings[i].regex != NULL ; i++) {
		assert(g_source_type_mappings[i].compiled);
		r = regexec(
			&g_source_type_mappings[i].preg, item->name,
			0 /* nmatches */, NULL /* pmatch */, 0 /* eflags */
		);
		if (r == REG_NOMATCH) {
			continue;
		}
		if (r == 0) {
			lis_log_info("Item '%s': type = %d (regex %d)",
				item->name, g_source_type_mappings[i].type, i);
			item->type = g_source_type_mappings[i].type;
			return LIS_OK;
		}
		lis_log_error("Regex %d has failed, code=%d !", i, r);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	lis_log_warning("Failed to identify type of item '%s'", item->name);
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


enum lis_error lis_api_normalizer_source_types(struct lis_api *to_wrap, struct lis_api **impl)
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
	lis_bw_set_clean_impl(*impl, clean_impl, NULL);
	return err;
}
