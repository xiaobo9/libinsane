#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "workaround_set_opt_late"


static const char *g_opt_to_set_late[] = {
	OPT_NAME_MODE,
	"bit_depth", // twain only
	NULL,
};


struct opt_value {
	struct lis_item *item;

	char *opt_name;
	union lis_value value;
	int string;

	struct opt_value *next;
};


static struct opt_value *g_values = NULL;


static struct opt_value *get_opt_value(struct lis_item *item, const char *name)
{
	struct opt_value *val;

	for (val = g_values; val != NULL ; val = val->next) {
		if (val->item == item
				&& strcasecmp(val->opt_name, name) == 0) {
			return val;
		}
	}
	return NULL;
}


static void free_value(struct opt_value *value)
{
	struct opt_value *pval;

	for (pval = g_values ; pval != NULL ; pval = pval->next) {
		if (pval->next == value) {
			break;
		}
	}

	if (pval == NULL) {
		g_values = value->next;
	} else {
		pval->next = value->next;
	}

	FREE(value->opt_name);
	if (value->string) {
		FREE(value->value.string);
	}
	memset(value, 0, sizeof(*value));
	FREE(value);
}


static enum lis_error get_value(struct lis_option_descriptor *self, union lis_value *value)
{
	struct lis_option_descriptor *original = lis_bw_get_original_opt(self);
	struct lis_item *item = lis_bw_opt_get_user_ptr(self);
	struct opt_value *late_value = get_opt_value(item, self->name);

	if (late_value != NULL) {
		memcpy(value, &late_value->value, sizeof(*value));
		return LIS_OK;
	}

	return original->fn.get_value(original, value);
}


static enum lis_error set_value(struct lis_option_descriptor *self, union lis_value value, int *set_flags)
{
	struct lis_item *item = lis_bw_opt_get_user_ptr(self);
	struct opt_value *late_value = get_opt_value(item, self->name);

	lis_log_info("Delaying update of option [%s]", self->name);

	if (late_value != NULL) {
		free_value(late_value);
	}

	late_value = calloc(1, sizeof(struct opt_value));
	if (late_value == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	late_value->item = item;
	late_value->opt_name = strdup(self->name);
	if (late_value->opt_name == NULL) {
		lis_log_error("Out of memory");
		FREE(late_value);
		return LIS_ERR_NO_MEM;
	}
	if (self->value.type == LIS_TYPE_STRING) {
		late_value->string = 1;
		late_value->value.string = strdup(value.string);
		if (late_value->value.string == NULL) {
			lis_log_error("Out of memory");
			FREE(late_value->opt_name);
			FREE(late_value);
			return LIS_ERR_NO_MEM;
		}
	} else {
		memcpy(&late_value->value, &value, sizeof(late_value->value));
	}
	late_value->next = g_values;
	g_values = late_value;
	*set_flags = 0;
	return LIS_OK;
}


static enum lis_error opt_filter(struct lis_item *item, struct lis_option_descriptor *desc, void *user_data)
{
	int i;

	LIS_UNUSED(user_data);

	for (i = 0 ; g_opt_to_set_late[i] != NULL ; i++) {
		if (strcasecmp(desc->name, g_opt_to_set_late[i]) == 0) {
			break;
		}
	}
	if (g_opt_to_set_late[i] == NULL) {
		// do not wrap
		return LIS_OK;
	}

	lis_log_info(
		"Wrapping option [%s] to delay setting its value",
		desc->name
	);
	lis_bw_opt_set_user_ptr(desc, item, NULL);
	desc->fn.set_value = set_value;
	desc->fn.get_value = get_value;
	return LIS_OK;
}


static enum lis_error on_scan_start(
		struct lis_item *item, struct lis_scan_session **session,
		void *user_data
	)
{
	struct opt_value *val;
	enum lis_error err;
	struct lis_item *original_item;
	struct lis_option_descriptor **opts;
	int set_flags;

	LIS_UNUSED(session);
	LIS_UNUSED(user_data);

	original_item = lis_bw_get_original_item(item);

	err = original_item->get_options(original_item, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get options: 0x%X, %s",
			err, lis_strerror(err)
		);
		return err;
	}

	lis_log_info("Setting late options ...");
	for ( ; *opts != NULL ; opts++) {
		val = get_opt_value(item, (*opts)->name);
		if (val == NULL) {
			continue;
		}
		lis_log_info(
			"Setting option [%s] late ...",
			val->opt_name
		);
		err = (*opts)->fn.set_value(
			(*opts), val->value, &set_flags
		);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to set option [%s] late:"
				" 0x%X, %s",
				val->opt_name, err, lis_strerror(err)
			);
			return err;
		}
	}

	return original_item->scan_start(original_item, session);
}


static void on_item_closed(struct lis_item *item, int root, void *user_data)
{
	struct opt_value *val, *nval;

	LIS_UNUSED(root);
	LIS_UNUSED(user_data);

	for (val = g_values,
			nval = (g_values != NULL) ? g_values->next : NULL ;
			val != NULL ;
			val = nval, nval = (nval != NULL ? nval->next : NULL)) {
		if (val->item == item) {
			free_value(val);
		}
	}
}


enum lis_error lis_api_workaround_set_opt_late(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_opt_desc_filter(*impl, opt_filter, NULL);
		lis_bw_set_on_scan_start(*impl, on_scan_start, NULL);
		lis_bw_set_on_close_item(*impl, on_item_closed, NULL);
	}

	return err;
}
