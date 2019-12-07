#include <assert.h>
#include <stdlib.h>

#include <wia.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "util.h"

BSTR lis_cstr2bstr(const char *s)
{
	size_t len;
	wchar_t *wchr;
	BSTR bstr;

	if (s == NULL) {
		return NULL;
	}

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

	if (bstr == NULL) {
		return NULL;
	}

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


enum lis_error hresult_to_lis_error(HRESULT hr) {
	switch (hr) {
		case S_OK: return LIS_OK;

		/* code we get when calling get_device() with an invalid ID */
		case WIA_S_NO_DEVICE_AVAILABLE: return LIS_ERR_INVALID_VALUE;

		case E_INVALIDARG: return LIS_ERR_INVALID_VALUE;
		case E_NOTIMPL: return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
		case E_OUTOFMEMORY: return LIS_ERR_NO_MEM;

		case REGDB_E_CLASSNOTREG:
			lis_log_warning(
				"Internal error: Class not registered"
			);
			break;
		default:
			lis_log_warning(
				"Unknown Windows error code: 0x%lX", hr
			);
			break;
	}

	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}
