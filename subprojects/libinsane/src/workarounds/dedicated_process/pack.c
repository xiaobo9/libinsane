#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "pack.h"

/* WORKAROUND(Jflesch):
 * In some cases, we simply want to use va_arg() to ignore some values.
 * But there is a bug with GCC >= 10: if we call va_arg() without storing
 * and using the value, GCC optimizes out the va_arg() statements.
 * So we don't go to the next element on the stack as expected, and the next
 * call to va_arg() gets the value we wanted to ignore.
 *
 * The easiest solution to avoid this optimization is to store the value
 * in a volatile variable.
 */
#define FORCE_VA_ARG(va, type) do { \
		volatile type _var = va_arg(va, type); \
		_var = _var; \
	} while(0)


/* depending on the architecture, va_list may be a struct, an array, or a plane.
 * --> just to be safe and avoid ambiguities, we wrap it in a struct.
 */
struct wrapped_va_list {
	va_list va;
};


typedef size_t (compute_size_fn)(struct wrapped_va_list *va);
typedef enum lis_error (serialize_fn)(void **out, struct wrapped_va_list *va);
typedef enum lis_error (deserialize_fn)(const void **in, struct wrapped_va_list *va);


static compute_size_fn compute_size_integer;
static serialize_fn serialize_integer;
static deserialize_fn deserialize_integer;

static compute_size_fn compute_size_double;
static serialize_fn serialize_double;
static deserialize_fn deserialize_double;

static compute_size_fn compute_size_string;
static serialize_fn serialize_string;
static deserialize_fn deserialize_string;

static compute_size_fn compute_size_ptr;
static serialize_fn serialize_ptr;
static deserialize_fn deserialize_ptr;

static compute_size_fn compute_size_value;
static serialize_fn serialize_value;
static deserialize_fn deserialize_value;


static struct {
	char id;
	compute_size_fn *compute_size;
	serialize_fn *serialize;
	deserialize_fn *deserialize;
} g_data_types[] = {
	{
		.id = 'i',
		.compute_size = compute_size_integer,
		.serialize = serialize_integer,
		.deserialize = deserialize_integer,
	},
	{
		.id = 'd',
		.compute_size = compute_size_integer,
		.serialize = serialize_integer,
		.deserialize = deserialize_integer,
	},
	{
		.id = 'f',
		.compute_size = compute_size_double,
		.serialize = serialize_double,
		.deserialize = deserialize_double,
	},
	{
		.id = 'p',
		.compute_size = compute_size_ptr,
		.serialize = serialize_ptr,
		.deserialize = deserialize_ptr,
	},
	{
		.id = 's',
		.compute_size = compute_size_string,
		.serialize = serialize_string,
		.deserialize = deserialize_string,
	},
	{
		.id = 'v',
		.compute_size = compute_size_value,
		.serialize = serialize_value,
		.deserialize = deserialize_value,
	}
};


static size_t compute_size_integer(struct wrapped_va_list *va)
{
	FORCE_VA_ARG(va->va, int); // NOLINT (clang false positive)
	return sizeof(int);
}


static enum lis_error serialize_integer(void **out, struct wrapped_va_list *va)
{
	int *ptr = *out;
	int val = va_arg(va->va, int); // NOLINT (clang false positive)

	*ptr = val;
	*out = ptr + 1;
	return LIS_OK;
}


static enum lis_error deserialize_integer(const void **in, struct wrapped_va_list *va)
{
	int *out = va_arg(va->va, int *); // NOLINT (clang false positive)
	const int *ptr = *in;

	*out = *ptr;
	*in = ptr + 1;
	return LIS_OK;
}


static size_t compute_size_double(struct wrapped_va_list *va)
{
	FORCE_VA_ARG(va->va, double); // NOLINT (clang false positive)
	return sizeof(double);
}


static enum lis_error serialize_double(void **out, struct wrapped_va_list *va)
{
	double *ptr = *out;
	double val = va_arg(va->va, double); // NOLINT (clang false positive)

	*ptr = val;
	*out = ptr + 1;
	return LIS_OK;
}


static enum lis_error deserialize_double(const void **in, struct wrapped_va_list *va)
{
	double *out = va_arg(va->va, double *); // NOLINT (clang false positive)
	const double *ptr = *in;

	*out = *ptr;
	*in = ptr + 1;
	return LIS_OK;
}


static size_t compute_size_ptr(struct wrapped_va_list *va)
{
	FORCE_VA_ARG(va->va, intptr_t); // NOLINT (clang false positive)
	return sizeof(intptr_t);
}


static enum lis_error serialize_ptr(void **out, struct wrapped_va_list *va)
{
	intptr_t *ptr = *out;
	intptr_t val = va_arg(va->va, intptr_t); // NOLINT (clang false positive)

	*ptr = val;
	*out = ptr + 1;
	return LIS_OK;
}


static enum lis_error deserialize_ptr(const void **in, struct wrapped_va_list *va)
{
	intptr_t *out = va_arg(va->va, intptr_t *); // NOLINT (clang false positive)
	const intptr_t *ptr = *in;

	*out = *ptr;
	*in = ptr + 1;
	return LIS_OK;
}


static size_t compute_size_string(struct wrapped_va_list *va)
{
	const char *str = va_arg(va->va, const char *); // NOLINT (clang false positive)
	if (str == NULL) {
		return 1;
	}
	return strlen(str) + 1;
}


static enum lis_error serialize_string(void **out, struct wrapped_va_list *va)
{
	const char *str = va_arg(va->va, const char *); // NOLINT (clang false positive)
	size_t l;

	if (str == NULL) {
		**((char **)out) = '\0';
		*out += 1;
		return LIS_OK;
	}

	l = strlen(str);
	strcpy(*out, str); // NOLINT
	*out += l + 1;

	return LIS_OK;
}


static enum lis_error deserialize_string(const void **in, struct wrapped_va_list *va)
{
	const char **out = va_arg(va->va, const char **); // NOLINT (clang false positive)
	const char *str = *in;
	size_t l = strlen(str);

	*out = str;
	*in += l + 1;
	return LIS_OK;
}


static size_t compute_size_value(struct wrapped_va_list *va)
{
	enum lis_value_type vtype = va_arg(va->va, enum lis_value_type); // NOLINT
	union lis_value value = va_arg(va->va, union lis_value);

	switch(vtype) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_INTEGER:
		case LIS_TYPE_IMAGE_FORMAT:
			return sizeof(int);
		case LIS_TYPE_DOUBLE:
			return sizeof(double);
		case LIS_TYPE_STRING:
			return strlen(value.string) + 1;
	}

	lis_log_error("Unexpected value type: %d\n", vtype);
	assert(0);
	return -1;
}


static enum lis_error serialize_value(void **out, struct wrapped_va_list *va)
{
	enum lis_value_type vtype = va_arg(va->va, enum lis_value_type); // NOLINT
	union lis_value value = va_arg(va->va, union lis_value);
	size_t l;

	switch(vtype) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_INTEGER:
		case LIS_TYPE_IMAGE_FORMAT:
			**((int **)out) = value.integer;
			*out += sizeof(value.integer);
			return LIS_OK;
		case LIS_TYPE_DOUBLE:
			**((double **)out) = value.dbl;
			*out += sizeof(value.dbl);
			return LIS_OK;
		case LIS_TYPE_STRING:
			l = strlen(value.string);
			strcpy(*out, value.string); // NOLINT
			*out += l + 1;
			return LIS_OK;
	}

	lis_log_error("Unexpected value type: %d\n", vtype);
	assert(0);
	return LIS_ERR_INVALID_VALUE;
}


static enum lis_error deserialize_value(const void **in, struct wrapped_va_list *va)
{
	enum lis_value_type vtype = va_arg(va->va, enum lis_value_type); // NOLINT
	union lis_value *value = va_arg(va->va, union lis_value *);
	size_t l;

	switch(vtype) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_INTEGER:
		case LIS_TYPE_IMAGE_FORMAT:
			value->integer = **((int **)in);
			*in += sizeof(int);
			return LIS_OK;
		case LIS_TYPE_DOUBLE:
			value->dbl = **((double **)in);
			*in += sizeof(double);
			return LIS_OK;
		case LIS_TYPE_STRING:
			l = strlen(*in);
			value->string = *in;
			*in += l + 1;
			return LIS_OK;
	}

	lis_log_error("Unexpected value type: %d\n", vtype);
	assert(0);
	return LIS_ERR_INVALID_VALUE;
}



size_t lis_compute_packed_size(const char *format, ...)
{
	size_t s = 0;
	struct wrapped_va_list va;
	unsigned int t;

	va_start(va.va, format);

	for (; format[0] != '\0' ; format++) {
		for (t = 0 ; t < LIS_COUNT_OF(g_data_types) ; t++) {
			if (format[0] != g_data_types[t].id) {
				continue;
			}

			s += g_data_types[t].compute_size(&va);
			break;
		}

		if (t >= LIS_COUNT_OF(g_data_types)) {
			lis_log_error("Unknown data type: %c", format[0]);
			return (size_t)-1;
		}
	}

	va_end(va.va);
	return s;
}


void lis_pack(void **out, const char *format, ...)
{
	struct wrapped_va_list va;
	unsigned int t;

	va_start(va.va, format);

	for (; format[0] != '\0' ; format++) {
		for (t = 0 ; t < LIS_COUNT_OF(g_data_types) ; t++) {
			if (format[0] != g_data_types[t].id) {
				continue;
			}

			g_data_types[t].serialize(out, &va);
			break;
		}

		if (t >= LIS_COUNT_OF(g_data_types)) {
			lis_log_error("Unknown data type: %c", format[0]);
			abort();
			return;
		}
	}

	va_end(va.va);
}


void lis_unpack(const void **in, const char *format, ...)
{
	struct wrapped_va_list va;
	unsigned int t;

	va_start(va.va, format);

	for (; format[0] != '\0' ; format++) {
		for (t = 0 ; t < LIS_COUNT_OF(g_data_types) ; t++) {
			if (format[0] != g_data_types[t].id) {
				continue;
			}

			g_data_types[t].deserialize(in, &va);
			break;
		}

		if (t >= LIS_COUNT_OF(g_data_types)) {
			lis_log_error("Unknown data type: %c", format[0]);
			abort();
			return;
		}
	}

	va_end(va.va);
}
