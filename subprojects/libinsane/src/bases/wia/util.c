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


static void print_unknown_hresult(HRESULT hr) {
	LPTSTR errorText = NULL;

	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&errorText, 0,
		NULL
	);

	if (errorText == NULL)
	{
		lis_log_error("Unknown Windows error code: 0x%lX", hr);
	} else {
		lis_log_error("Unknown Windows error code: 0x%lX, %s", hr, errorText);
		LocalFree(errorText);
		errorText = NULL;
	}
}

enum lis_error hresult_to_lis_error(HRESULT hr) {
	switch (hr) {
		case S_OK: return LIS_OK;

		/* code we get when calling get_device() with an invalid ID */
		case WIA_S_NO_DEVICE_AVAILABLE: return LIS_ERR_INVALID_VALUE;
		case WIA_ERROR_PAPER_JAM: return LIS_ERR_JAMMED;
		case WIA_ERROR_PAPER_PROBLEM: return LIS_ERR_JAMMED;
		case WIA_ERROR_BUSY: return LIS_ERR_DEVICE_BUSY;
		case WIA_ERROR_WARMING_UP: return LIS_WARMING_UP;
		case WIA_ERROR_DEVICE_LOCKED: return LIS_ERR_HW_IS_LOCKED;

		case E_INVALIDARG: return LIS_ERR_INVALID_VALUE;
		case E_NOTIMPL: return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
		case E_OUTOFMEMORY: return LIS_ERR_NO_MEM;

		case REGDB_E_CLASSNOTREG:
			lis_log_warning(
				"Internal error: Class not registered"
			);
			break;
		default:
			print_unknown_hresult(hr);
			break;
	}

	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}
