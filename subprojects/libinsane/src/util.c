#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "version.h"


enum lis_error lis_copy(const enum lis_value_type type, const union lis_value *original, union lis_value *copy)
{
	switch(type) {
		case LIS_TYPE_BOOL:
			copy->boolean = original->boolean;
			return LIS_OK;
		case LIS_TYPE_INTEGER:
			copy->integer = original->integer;
			return LIS_OK;
		case LIS_TYPE_DOUBLE:
			copy->dbl = original->dbl;
			return LIS_OK;
		case LIS_TYPE_STRING:
			FREE(copy->string);
			copy->string = strdup(original->string);
			return copy->string != NULL ? LIS_OK : LIS_ERR_NO_MEM;
		case LIS_TYPE_IMAGE_FORMAT:
			copy->format = original->format;
			return LIS_OK;
	}
	lis_log_error("Unknown value type: %d !", type);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}


void lis_free(const enum lis_value_type type, union lis_value *value)
{
	if (type == LIS_TYPE_STRING) {
		FREE(value->string);
	}
}


int lis_compare(enum lis_value_type type, union lis_value val1, union lis_value val2)
{
	switch(type) {
		case LIS_TYPE_BOOL:
			return val1.boolean == val2.boolean;
		case LIS_TYPE_INTEGER:
			return val1.integer == val2.integer;
		case LIS_TYPE_DOUBLE:
			return val1.dbl == val2.dbl;
		case LIS_TYPE_STRING:
			return strcasecmp(val1.string, val2.string) == 0;
		case LIS_TYPE_IMAGE_FORMAT:
			return val1.format == val2.format;
	}
	lis_log_error("Unknown value type: %d !", type);
	return 0;
}


union lis_value lis_add(enum lis_value_type type, union lis_value a, union lis_value b)
{
	union lis_value out;

	switch(type) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
		case LIS_TYPE_INTEGER:
			out.integer = a.integer + b.integer;
			return out;
		case LIS_TYPE_DOUBLE:
			out.dbl = a.dbl + b.dbl;
			return out;
	}

	lis_log_error("Can't add type %d", type);
	assert(0);
	out.integer = 0;
	return out;
}


union lis_value lis_sub(enum lis_value_type type, union lis_value a, union lis_value b)
{
	union lis_value out;

	switch(type) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_STRING:
		case LIS_TYPE_IMAGE_FORMAT:
			break;
		case LIS_TYPE_INTEGER:
			out.integer = a.integer - b.integer;
			return out;
		case LIS_TYPE_DOUBLE:
			out.dbl = a.dbl - b.dbl;
			return out;
	}

	lis_log_error("Can't substract type %d", type);
	assert(0);
	out.integer = 0;
	return out;
}


int lis_getenv(const char *var, int default_val)
{
	const char *val_str;

	val_str = getenv(var);
	if (val_str == NULL) {
		return default_val;
	}
	return atoi(val_str);
}


const char *lis_get_version(void)
{
	return LIBINSANE_VERSION;
}


void lis_hexdump(const void *_data, size_t nb_bytes)
{
	const uint8_t *data = _data;

	while(nb_bytes > 0) {
		lis_log_debug(
			"[HEX] (%4d) 0x %02X %02X %02X %02X || %02X %02X %02X %02X",
			(int)nb_bytes,
			((int)(data[0])) & 0xFF,
			(nb_bytes >= 2 ? (((int)(data[1])) & 0xFF) : 0x00),
			(nb_bytes >= 3 ? (((int)(data[2])) & 0xFF) : 0x00),
			(nb_bytes >= 4 ? (((int)(data[3])) & 0xFF) : 0x00),
			(nb_bytes >= 5 ? (((int)(data[4])) & 0xFF) : 0x00),
			(nb_bytes >= 6 ? (((int)(data[5])) & 0xFF) : 0x00),
			(nb_bytes >= 7 ? (((int)(data[6])) & 0xFF) : 0x00),
			(nb_bytes >= 8 ? (((int)(data[7])) & 0xFF) : 0x00)
		);
		data += 8;
		if (nb_bytes < 8) {
			nb_bytes = 0;
		} else {
			nb_bytes -= 8;
		}
	}
}


enum lis_error lis_set_option(
		struct lis_item *item, const char *opt_name,
		const char *opt_value
	)
{
	char *endptr = NULL;
	struct lis_option_descriptor **opts;
	enum lis_error err;
	union lis_value value;
	int set_flags = -1;

	assert(item != NULL);
	assert(opt_name != NULL);
	assert(opt_value != NULL);

	lis_log_info("%s: Setting %s=%s", item->name, opt_name, opt_value);

	err = item->get_options(item, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s: Failed to list options: 0x%X, %s",
			item->name, err, lis_strerror(err)
		);
		return err;
	}

	for ( ; (*opts) != NULL ; opts++) {
		if (strcasecmp(opt_name, (*opts)->name) == 0) {
			break;
		}
	}

	if ((*opts) == NULL) {
		lis_log_error("%s: Option '%s' not found", item->name, opt_name);
		return LIS_ERR_INVALID_VALUE;
	}

	memset(&value, 0, sizeof(value));
	switch((*opts)->value.type) {
		case LIS_TYPE_BOOL:
			if (strcmp(opt_value, "1") == 0
					|| strcasecmp(opt_value, "true") == 0) {
				value.boolean = 1;
			}
			break;
		case LIS_TYPE_INTEGER:
			value.integer = strtol(opt_value, &endptr, 10);
			if (endptr == NULL || endptr[0] != '\0') {
				lis_log_error(
					"Option %s->%s expected an integer"
					" value ('%s' is not an integer)",
					item->name, opt_name, opt_value
				);
				return LIS_ERR_INVALID_VALUE;
			}
			break;
		case LIS_TYPE_DOUBLE:
			value.dbl = strtod(opt_value, &endptr);
			if (endptr == NULL || endptr[0] != '\0') {
				lis_log_error(
					"Option %s->%s expected a double"
					" ('%s' is not an double)",
					item->name, opt_name, opt_value
				);
				return LIS_ERR_INVALID_VALUE;
			}
			break;
		case LIS_TYPE_STRING:
			value.string = opt_value;
			break;
		case LIS_TYPE_IMAGE_FORMAT:
			lis_log_error(
				"%s: Setting image format option is not"
				" supported", item->name
			);
			return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
	}

	err = (*opts)->fn.set_value(*opts, value, &set_flags);
	if (LIS_IS_OK(err)) {
		lis_log_info(
			"%s: Successfully set %s=%s (flags=0x%X)",
			item->name, opt_name, opt_value, set_flags
		);
	} else {
		lis_log_error(
			"%s: Failed to set %s=%s",
			item->name, opt_name, opt_value
		);
	}
	return err;
}
