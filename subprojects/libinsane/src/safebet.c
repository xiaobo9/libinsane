#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/dumb.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/multiplexer.h>
#include <libinsane/normalizers.h>
#include <libinsane/safebet.h>
#include <libinsane/util.h>
#include <libinsane/workarounds.h>

#ifdef OS_LINUX
#include <libinsane/sane.h>
#endif

#ifdef OS_WINDOWS
#include <libinsane/twain.h>
#include <libinsane/wia_ll.h>
#endif


static const struct {
	const char *name;
	const char *env;
	enum lis_error (*wrap_cb)(struct lis_api *to_wrap, struct lis_api **wrapper);
	int enabled_by_default;
} g_implementations[] = {
	{
		.name = "workaround_check_capabilities",
		.env = "LIBINSANE_WORKAROUND_CHECK_CAPABILITIES",
		.wrap_cb = lis_api_workaround_check_capabilities,
		.enabled_by_default = 1,
	},
	{
		.name = "workaround_cache",
		.env = "LIBINSANE_WORKAROUND_CACHE",
		.wrap_cb = lis_api_workaround_cache,
		.enabled_by_default = 1,
	},
	{
		.name = "workaround_lamp",
		.env = "LIBINSANE_WORKAROUND_LAMP",
		.wrap_cb = lis_api_workaround_lamp,
#ifdef OS_WINDOWS
		.enabled_by_default = 0, // Sane only
#else
		.enabled_by_default = 1,
#endif
	},
	{
		.name = "normalizer_opt_aliases",
		.env = "LIBINSANE_NORMALIZER_OPT_ALIASES",
		.wrap_cb = lis_api_normalizer_opt_aliases,
#ifdef OS_WINDOWS
		.enabled_by_default = 1,
#else
		.enabled_by_default = 0,
#endif
	},
	{
		.name = "workaround_opt_values",
		.env = "LIBINSANE_WORKAROUND_OPT_VALUES",
		.wrap_cb = lis_api_workaround_opt_values,
		.enabled_by_default = 1,
	},
	{
		.name = "workaround_opt_names",
		.env = "LIBINSANE_WORKAROUND_OPT_NAMES",
		.wrap_cb = lis_api_workaround_opt_names,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_bmp2raw",
		.env = "LIBINSANE_NORMALIZER_BMP2RAW",
		.wrap_cb = lis_api_normalizer_bmp2raw,
#ifdef OS_WINDOWS
		.enabled_by_default = 1, /* WIA returns BMP images */
#else
		.enabled_by_default = 0, /* Sane returns various RAW formats */
#endif
	},
	{
		.name = "normalizer_raw24",
		.env = "LIBINSANE_NORMALIZER_RAW24",
		.wrap_cb = lis_api_normalizer_raw24,
#ifdef OS_WINDOWS
		.enabled_by_default = 0, /* WIA returns BMP images */
#else
		.enabled_by_default = 1, /* Sane returns various RAW formats */
#endif
	},
	{
		.name = "normalizer_resolution",
		.env = "LIBINSANE_NORMALIZER_RESOLUTION",
		.wrap_cb = lis_api_normalizer_resolution,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_clean_dev_descs",
		.env = "LIBINSANE_NORMALIZER_CLEAN_DEV_DESCS",
		.wrap_cb = lis_api_normalizer_clean_dev_descs,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_safe_defaults",
		.env = "LIBINSANE_NORMALIZER_SAFE_DEFAULTS",
		.wrap_cb = lis_api_normalizer_safe_defaults,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_source_nodes",
		.env = "LIBINSANE_NORMALIZER_SOURCE_NODES",
		.wrap_cb = lis_api_normalizer_source_nodes,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_min_one_source",
		.env = "LIBINSANE_NORMALIZER_MIN_ONE_SOURCE",
		.wrap_cb = lis_api_normalizer_min_one_source,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_source_names",
		.env = "LIBINSANE_NORMALIZER_SOURCE_NAMES",
		.wrap_cb = lis_api_normalizer_source_names,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_source_types",
		.env = "LIBINSANE_NORMALIZER_SOURCE_TYPES",
		.wrap_cb = lis_api_normalizer_source_types,
		.enabled_by_default = 1,
	},
	{
		.name = "workaround_one_page_flatbed",
		.env = "LIBINSANE_WORKAROUND_ONE_PAGE_FLATBED",
		.wrap_cb = lis_api_workaround_one_page_flatbed,
		.enabled_by_default = 1,
	},
	{
		.name = "normalizer_all_opts_on_all_sources",
		.env = "LIBINSANE_NORMALIZER_ALL_OPTS_ON_ALL_SOURCES",
		.wrap_cb = lis_api_normalizer_all_opts_on_all_sources,
		.enabled_by_default = 1,
	},
	{	// dedicated thread wrapper should be loaded last
		.name = "workaround_dedicated_thread",
		.env = "LIBINSANE_WORKAROUND_DEDICATED_THREAD",
		.wrap_cb = lis_api_workaround_dedicated_thread,
		.enabled_by_default = 1,
	},
};


enum lis_error lis_safebet(struct lis_api **out_impls)
{
	enum lis_error err = LIS_ERR_UNSUPPORTED;
	struct lis_api *impls[4] = { NULL };
	int nb_impls = 0;
	struct lis_api *next;
	size_t i;
	int env;

	*out_impls = NULL;

	lis_log_info("Initializing base implementations ...");

#ifdef OS_LINUX
	if (lis_getenv("LIBINSANE_SANE", 1)) {
		err = lis_api_sane(&impls[nb_impls]);
		if (LIS_IS_ERROR(err)) {
			goto err_impls;
		}
		nb_impls++;
	}
#endif

#ifdef OS_WINDOWS
	if (lis_getenv("LIBINSANE_TWAIN", 1)) {
		err = lis_api_twain(&impls[nb_impls]);
		if (LIS_IS_ERROR(err)) {
			goto err_impls;
		}
		nb_impls++;
	}

	if (lis_getenv("LIBINSANE_WIA_LL", 1)) {
		err = lis_api_wia_ll(&impls[nb_impls]);
		if (LIS_IS_ERROR(err)) {
			goto err_impls;
		}
		nb_impls++;
	}
#endif

	if (lis_getenv("LIBINSANE_DUMB", nb_impls == 0)) {
		err = lis_api_dumb(&impls[nb_impls], "dumb");
		if (LIS_IS_ERROR(err)) {
			goto err_impls;
		}
		nb_impls++;
	}

	err = lis_api_multiplexer(impls, nb_impls, &next);
	if (LIS_IS_ERROR(err)) {
		goto err_impls;
	}
	*out_impls = next;

	lis_log_info("%d base implementations initialized", nb_impls);

	lis_log_info("Initializing workarounds & normalizers ...");
	nb_impls = 0;
	for (i = 0 ; i < LIS_COUNT_OF(g_implementations) ; i++) {
		env = lis_getenv(g_implementations[i].env, g_implementations[i].enabled_by_default);
		lis_log_info("%s=%d", g_implementations[i].env, env);
		if (env) {
			err = g_implementations[i].wrap_cb(*out_impls, &next);
			if (LIS_IS_ERROR(err)) {
				lis_log_error("Failed to initialize '%s'",
						g_implementations[i].name);
				goto error;
			}
			*out_impls = next;
			nb_impls++;
		}
	}
	lis_log_info("%d workarounds & normalizers initialized", nb_impls);

	return err;

error:
	(*out_impls)->cleanup(*out_impls);
	*out_impls = NULL;
	return err;

err_impls:
	for (nb_impls-- ; nb_impls >= 0 ; nb_impls--) {
		impls[nb_impls]->cleanup(impls[nb_impls]);
	}
	return err;
}
