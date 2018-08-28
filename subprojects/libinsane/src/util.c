#include <assert.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/util.h>


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
