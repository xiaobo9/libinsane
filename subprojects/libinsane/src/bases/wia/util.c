#include <assert.h>
#include <stdlib.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "util.h"

BSTR lis_cstr2bstr(const char *s)
{
	size_t len;
	wchar_t *wchr;
	BSTR bstr;

	wchr = calloc(strlen(s) + 4, sizeof(wchar_t));
	if (wchr == NULL) {
		lis_log_error("Out of memory");
		return NULL;
	}

	len = mbstowcs(wchr, s, strlen(s));
	if (len == (size_t)-1) {
		lis_log_error(
			"Failed to convert wide chars from string (%s)",
			s
		);
		FREE(wchr);
		return NULL;
	}

	bstr = SysAllocString(wchr);
	FREE(wchr);

	return bstr;
}


char *lis_bstr2cstr(BSTR bstr)
{
	wchar_t *wchr;
	size_t len;
	char *out;

	wchr = bstr;
	len = wcslen(wchr);
	out = calloc(len + 1, sizeof(char));
	if (out == NULL) {
		lis_log_error("Out of memory");
		return NULL;
	}

	len = wcstombs(out, wchr, len);
	if (len == (size_t)-1) {
		lis_log_error("Failed to convert string from wide char");
		FREE(out);
		return NULL;
	}

	return out;
}


char *lis_propvariant2char(PROPVARIANT *prop)
{
	assert(prop->vt == VT_BSTR);
	return lis_bstr2cstr(prop->bstrVal);
}
