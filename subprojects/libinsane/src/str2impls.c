#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/dumb.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/str2impls.h>
#include <libinsane/util.h>
#include <libinsane/workarounds.h>

#ifdef OS_LINUX
#include <libinsane/sane.h>
#endif

#ifdef OS_WINDOWS
#include <libinsane/twain.h>
#include <libinsane/wia_automation.h>
#include <libinsane/wia_ll.h>
#endif

enum lis_error lis_str2impls(const char *list_of_impls, struct lis_api **impls)
{
	enum lis_error err = LIS_OK;
	char *input_str;
	char *save_ptr = NULL;
	const char *tok;
	struct lis_api *next;

	lis_log_debug("enter");

	input_str = strdup(list_of_impls);
	if (input_str == NULL) {
		lis_log_debug("error no mem");
		return LIS_ERR_NO_MEM;
	}

	*impls = NULL;

	for (tok = strtok_r(input_str, ",", &save_ptr) ;
			tok != NULL ;
			tok = strtok_r(NULL, ",", &save_ptr)) {
		if (*impls == NULL) {

			// look for a base API
			if (strcmp(tok, "dumb") == 0) {
				err = lis_api_dumb(&next, "dumb");
#ifdef OS_LINUX
			} else if (strcmp(tok, "sane") == 0) {
				err = lis_api_sane(&next);
#endif
#ifdef OS_WINDOWS
			} else if (strcmp(tok, "twain") == 0) {
				err = lis_api_twain(&next);
			} else if (strcmp(tok, "wia_automation") == 0) {
				err = lis_api_wia_automation(&next);
			} else if (strcmp(tok, "wia_ll") == 0) {
				err = lis_api_wia_ll(&next);
#endif
			} else {
				lis_log_error("Unknown base API: %s", tok);
				err = LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
				goto error;
			}

		} else {

			// look for a wrapper
			// -> normalizers
			if (strcmp(tok, "all_opts_on_all_sources") == 0) {
				err = lis_api_normalizer_all_opts_on_all_sources(*impls, &next);
			} else if (strcmp(tok, "min_one_source") == 0) {
				err = lis_api_normalizer_min_one_source(*impls, &next);
			} else if (strcmp(tok, "bmp2raw") == 0) {
				err = lis_api_normalizer_bmp2raw(*impls, &next);
			} else if (strcmp(tok, "raw24") == 0) {
				err = lis_api_normalizer_raw24(*impls, &next);
			} else if (strcmp(tok, "resolution") == 0) {
				err = lis_api_normalizer_resolution(*impls, &next);
			} else if (strcmp(tok, "opt_aliases") == 0) {
				err = lis_api_normalizer_opt_aliases(*impls, &next);
			} else if (strcmp(tok, "source_nodes") == 0) {
				err = lis_api_normalizer_source_nodes(*impls, &next);
			} else if (strcmp(tok, "source_types") == 0) {
				err = lis_api_normalizer_source_types(*impls, &next);
			} else if (strcmp(tok, "safe_defaults") == 0) {
				err = lis_api_normalizer_safe_defaults(*impls, &next);
			} else if (strcmp(tok, "clean_dev_descs") == 0) {
				err = lis_api_normalizer_clean_dev_descs(*impls, &next);
			}
			// -> workarounds
			else if (strcmp(tok, "dedicated_thread") == 0) {
				err = lis_api_workaround_dedicated_thread(*impls, &next);
			} else if (strcmp(tok, "check_capabilities") == 0) {
				err = lis_api_workaround_check_capabilities(*impls, &next);
			} else if (strcmp(tok, "opt_names") == 0) {
				err = lis_api_workaround_opt_names(*impls, &next);
			} else if (strcmp(tok, "opt_values") == 0) {
				err = lis_api_workaround_opt_values(*impls, &next);
			} else {
				lis_log_error("Unknown API wrapper: %s", tok);
				err = LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
				goto error;
			}
		}

		if (LIS_IS_ERROR(err)) {
			lis_log_error("Failed to instanciate API implementation '%s'", tok);
			goto error;
		}

		*impls = next;
	}

	free(input_str);
	lis_log_debug("leave");
	return LIS_OK;

error:
	lis_log_debug("error");
	if (*impls != NULL) {
		(*impls)->cleanup(*impls);
	}
	free(input_str);
	return err;
}
