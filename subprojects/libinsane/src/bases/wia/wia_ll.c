#include <stdlib.h>

#include <initguid.h>
#include <objbase.h>
#include <wia.h>
#include <windows.h>
#include <unknwn.h>

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
	switch(hr) {
		case S_OK:
			break;
		default:
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
			return err;
	}

	return LIS_OK;
}


static void wiall_cleanup(struct lis_api *self)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);

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
}


static enum lis_error wiall_list_devices(
		struct lis_api *self, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);
	enum lis_error err;
	LIS_UNUSED(self);
	LIS_UNUSED(locs);
	LIS_UNUSED(dev_infos);

	if (private->wia_dev_mgr == NULL) {
		err = wiall_init(private);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
	}

	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
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

	if (private->wia_dev_mgr == NULL) {
		err = wiall_init(private);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
	}

	// TODO
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
