#include <assert.h>
#include <stdlib.h>

#include <comutil.h>
#include <initguid.h>
#include <objbase.h>
#include <propvarutil.h>
#include <sti.h>
#include <unknwn.h>
#include <wia.h>
#include <windows.h>

#include <libinsane/capi.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/wia_ll.h>
#include <libinsane/util.h>

#include "wia2.h"

#define NAME "wia2ll"

/* Regarding memory allocations, for consistency:
 * - Libinsane structures are allocated with calloc() and freed with free()
 * - COM/WIA2 objects are allocated with GlobalAlloc() and freed with
 *   GlobalFree()
 */

struct wiall_impl_private {
	struct lis_api parent;

	int com_init;
	struct lis_device_descriptor *descs;
	struct lis_device_descriptor **descs_ptrs;

	struct LisIWiaDevMgr2 *wia_dev_mgr;
};
#define WIALL_IMPL_PRIVATE(impl) ((struct wiall_impl_private *)(impl))


static void wiall_cleanup(struct lis_api *self);
static enum lis_error wiall_list_devices(
	struct lis_api *self, enum lis_device_locations locs,
	struct lis_device_descriptor ***dev_infos
);
static enum lis_error wiall_get_device(
	struct lis_api *self, const char *dev_id, struct lis_item **item
);

static struct lis_api g_impl_template = {
	.base_name = NAME,
	.cleanup = wiall_cleanup,
	.list_devices = wiall_list_devices,
	.get_device = wiall_get_device,
};


static enum lis_error hresult_to_lis_error(HRESULT hr) {
	switch (hr) {
		case S_OK:
			return LIS_OK;
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


static enum lis_error wiall_init(struct wiall_impl_private *private)
{
	HRESULT hr;
	enum lis_error err;

	lis_log_debug("wiall_init() ...");

	if (!private->com_init) {
		lis_log_debug("CoInitializeEx() ...");
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		lis_log_debug("CoInitializeEx(): 0x%lX", hr);
		switch(hr) {
			case S_OK:
				private->com_init = 1;
				break;
			case S_FALSE:
				// someone else already initialized it
				private->com_init = 0;
				lis_log_warning(
					"CoInitializeEx(): already initialized"
				);
				break;
			default:
				err = hresult_to_lis_error(hr);
				lis_log_error(
					"CoInitializeEx() failed: 0x%lX -> 0x%X (%s)",
					hr, err, lis_strerror(err)
				);
				lis_log_debug("wiall_init() failed");
				return err;
		}
	}

	lis_log_debug("CoCreateInstance(IWiaDevMgr2) ...");
	hr = CoCreateInstance(
		&CLSID_LisWiaDevMgr2,
		NULL, // pUnkOuter
		CLSCTX_LOCAL_SERVER,
		&IID_LisIWiaDevMgr2,
		(void **)(&(private->wia_dev_mgr))
	);
	lis_log_debug("CoCreateInstance(IWiaDevMgr2): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"CoCreateInstance() failed: 0x%lX -> 0x%X (%s)",
			hr, err, lis_strerror(err)
		);
		if (private->com_init) {
			lis_log_debug("CoUninitialize() ...");
			CoUninitialize();
			lis_log_debug("CoUninitialize() done");
		}
		private->com_init = 0;
		lis_log_debug("wiall_init() failed");
		return err;
	}

	lis_log_debug("wiall_init() done");
	return LIS_OK;
}


static void cleanup_dev_descs(struct wiall_impl_private *private)
{
	int i;

	if (private->descs_ptrs != NULL) {
		for (i = 0 ; private->descs_ptrs[i] != NULL ; i++) {
			FREE(private->descs[i].dev_id);
			FREE(private->descs[i].vendor);
			FREE(private->descs[i].model);
			FREE(private->descs[i].type);
		}
		FREE(private->descs_ptrs);
	}
	FREE(private->descs);
}


static void wiall_cleanup(struct lis_api *self)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);

	lis_log_debug("wiall_cleanup() ...");
	cleanup_dev_descs(private);
	if (private->wia_dev_mgr != NULL) {
		lis_log_debug("IWiaDevMgr2->Release() ...");
		private->wia_dev_mgr->lpVtbl->Release(
			private->wia_dev_mgr
		);
		lis_log_debug("IWiaDevMgr2->Release() done");
		private->wia_dev_mgr = NULL;
	}
	if (private->com_init) {
		lis_log_debug("CoUninitialize() ...");
		CoUninitialize();
		lis_log_debug("CoUninitialize() done");
	}
	private->com_init = 0;
	FREE(private);
	lis_log_debug("wiall_cleanup() done");
}


static char *propvariant2char(PROPVARIANT *prop)
{
	wchar_t *wchr;
	size_t len;
	char *out;

	assert(prop->vt == VT_BSTR);

	wchr = prop->bstrVal;
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


static enum lis_error get_device_descriptor(
		IWiaPropertyStorage *in_props,
		struct lis_device_descriptor *out_props
	)
{
	enum lis_error err;
	HRESULT hr;
	PROPSPEC input[] = {
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_DIP_DEV_ID,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_DIP_VEND_DESC,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_DIP_DEV_NAME,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_DIP_DEV_TYPE,
		},
	};
	PROPVARIANT output[LIS_COUNT_OF(input)];

	hr = in_props->lpVtbl->ReadMultiple(
		in_props,
		LIS_COUNT_OF(output),
		input, output
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"ReadMultiple failed: 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		return err;
	}

	out_props->model = propvariant2char(&output[2]);

	assert(output[3].vt == VT_I4);

	switch(GET_STIDEVICE_TYPE(output[3].lVal)) {
		case StiDeviceTypeDefault:
			out_props->type = "unknown";
			break;
		case StiDeviceTypeScanner:
			out_props->type = "scanner";
			break;
		default:
			lis_log_warning(
				"Device '%s' is not a scanner. Ignored",
				out_props->model
			);
			FREE(out_props->model);
			return LIS_OK;
	}

	out_props->dev_id = propvariant2char(&output[0]);
	out_props->vendor = propvariant2char(&output[1]);

	return LIS_OK;
}


static enum lis_error wiall_list_devices(
		struct lis_api *self, enum lis_device_locations locs,
		struct lis_device_descriptor ***lis_dev_infos
	)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);
	long wia_flags = 0; // all devices
	IEnumWIA_DEV_INFO *wia_dev_infos = NULL;
	IWiaPropertyStorage *props = NULL;
	enum lis_error err;
	unsigned long nb_devs;
	unsigned long nb_props;
	unsigned int i;
	HRESULT hr;

	lis_log_debug("wiall_list_devices() ...");

	cleanup_dev_descs(private);

	if (private->wia_dev_mgr == NULL) {
		err = wiall_init(private);
		if (LIS_IS_ERROR(err)) {
			lis_log_debug("wiall_list_devices() failed");
			return err;
		}
	}

	switch(locs) {
		case LIS_DEVICE_LOCATIONS_ANY:
			wia_flags = 0; // all devices
			break;
		case LIS_DEVICE_LOCATIONS_LOCAL_ONLY:
			wia_flags = WIA_DEVINFO_ENUM_LOCAL;
			break;
	}

	lis_log_debug("EnumDeviceInfo() ...");
	hr = private->wia_dev_mgr->lpVtbl->EnumDeviceInfo(
		private->wia_dev_mgr,
		wia_flags,
		&wia_dev_infos
	);
	lis_log_debug("EnumDeviceInfo(): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"EnumDeviceInfo() failed: 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		return err;
	}

	lis_log_debug("IEnumWIA_DEV_INFO->GetCount() ...");
	hr = wia_dev_infos->lpVtbl->GetCount(wia_dev_infos, &nb_devs);
	lis_log_debug("IEnumWIA_DEV_INFO->GetCount(): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"IEnumWIA_DEV_INFO->GetCount() failed:"
			" 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		wia_dev_infos->lpVtbl->Release(wia_dev_infos);
		lis_log_debug("wiall_list_devices() failed");
		return err;
	}

	lis_log_info("WIA2: Found %lu devices", nb_devs);

	private->descs_ptrs = calloc(
		nb_devs + 1, sizeof(struct lis_device_descriptor *)
	);
	if (nb_devs == 0) {
		*lis_dev_infos = private->descs_ptrs;
		wia_dev_infos->lpVtbl->Release(wia_dev_infos);
		lis_log_debug("wiall_list_devices() done");
		return LIS_OK;
	}

	private->descs = calloc(nb_devs, sizeof(struct lis_device_descriptor));
	if (private->descs == NULL || private->descs_ptrs == NULL) {
		lis_log_error("Out of memory");
		FREE(private->descs);
		FREE(private->descs_ptrs);
		wia_dev_infos->lpVtbl->Release(wia_dev_infos);
		return LIS_ERR_NO_MEM;
	}

	for (i = 0 ; i < nb_devs ; i++) {
		lis_log_debug("IEnumWIA_DEV_INFO->Next() ...");
		props = NULL;
		hr = wia_dev_infos->lpVtbl->Next(
			wia_dev_infos,
			1, // celt ; number of elements
			&props,
			&nb_props
		);
		lis_log_debug("IEnumWIA_DEV_INFO->Next(): 0x%lX", hr);
		if (FAILED(hr)) {
			err = hresult_to_lis_error(hr);
			lis_log_error(
				"IEnumWIA_DEV_INFO->Next() failed:"
				" 0x%lX -> 0x%X, %s"
				" (got %u elements, expected %lu)",
				hr, err, lis_strerror(err),
				i, nb_devs
			);
			wia_dev_infos->lpVtbl->Release(wia_dev_infos);
			lis_log_debug("wiall_list_devices() failed");
			return err;
		}
		assert(props != NULL);
		assert(nb_props == 1);

		err = get_device_descriptor(props, &private->descs[i]);
		if (LIS_IS_ERROR(err)) {
			lis_log_debug("wiall_list_devices() failed");
			return err;
		}
		if (private->descs[i].dev_id == NULL) {
			// valid but to ignore (not a scanner)
			i--;
			nb_devs--;
			continue;
		}

		private->descs_ptrs[i] = &private->descs[i];
	}
	lis_log_info("WIA2: %lu devices kept", nb_devs);

	wia_dev_infos->lpVtbl->Release(wia_dev_infos);

	*lis_dev_infos = private->descs_ptrs;
	lis_log_debug("wiall_list_devices() done");
	return LIS_OK;
}


static enum lis_error wiall_get_device(
		struct lis_api *self, const char *dev_id,
		struct lis_item **item
	)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);
	enum lis_error err;

	LIS_UNUSED(dev_id);
	LIS_UNUSED(item);

	lis_log_debug("wiall_get_device(%s) ...", dev_id);

	if (private->wia_dev_mgr == NULL) {
		err = wiall_init(private);
		if (LIS_IS_ERROR(err)) {
			lis_log_debug("wiall_get_device(%s) failed", dev_id);
			return err;
		}
	}

	// TODO
	lis_log_debug("wiall_get_device(%s) done", dev_id);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


enum lis_error lis_api_wia_ll(struct lis_api **impl)
{
	struct wiall_impl_private *private;

	// Reminder: dedicated_thread workaround does not apply here
	// --> no thread safety guaranteed
	// --> better limit the initialization done here

	private = calloc(1, sizeof(struct wiall_impl_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(&private->parent, &g_impl_template, sizeof(private->parent));
	*impl = &private->parent;
	return LIS_OK;
}
