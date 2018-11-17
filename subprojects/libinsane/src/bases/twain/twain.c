#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

#include <libinsane/capi.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/twain.h>
#include <libinsane/util.h>


#include "twain.h"


#define NAME "twain"
#define TWAIN_DSM_DLL "twaindsm.dll"
#define TWAIN_DSM_ENTRY_FN_NAME "DSM_Entry"


struct lis_twain_private {
	struct lis_api parent;

	bool init;

	HMODULE twain_dll;
	DSMENTRYPROC dsm_entry_fn;
};
#define LIS_TWAIN_PRIVATE(impl) ((struct lis_twain_private *)(impl))


static void twain_cleanup(struct lis_api *impl);
static enum lis_error twain_list_devices(
	struct lis_api *impl, enum lis_device_locations locs,
	struct lis_device_descriptor ***dev_infos
);
static enum lis_error twain_get_device(
	struct lis_api *impl, const char *dev_id,
	struct lis_item **item
);


static struct lis_api g_twain_template = {
	.base_name = NAME,
	.cleanup = twain_cleanup,
	.list_devices = twain_list_devices,
	.get_device = twain_get_device,
};


static enum lis_error twain_init(struct lis_twain_private *private)
{
	lis_log_debug("TWAIN init: LoadLibrary(" TWAIN_DSM_DLL ")");
	private->twain_dll = LoadLibraryA(TWAIN_DSM_DLL);
	if (private->twain_dll == NULL) {
		lis_log_error(
			"Failed to load Twain DLL: 0x%lX", GetLastError()
		);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}
	lis_log_debug("TWAIN init: LoadLibrary(" TWAIN_DSM_DLL ") successful");

	lis_log_debug(
		"TWAIN init: " TWAIN_DSM_DLL "->GetProcAddress("
		TWAIN_DSM_ENTRY_FN_NAME ")"
	);
	private->dsm_entry_fn = (DSMENTRYPROC)GetProcAddress(
		private->twain_dll, TWAIN_DSM_ENTRY_FN_NAME
	);
	if (private->dsm_entry_fn == NULL) {
		lis_log_error(
			"Failed to load Twain DSM_Entry function: 0x%lX",
			GetLastError()
		);
		FreeLibrary(private->twain_dll);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}
	lis_log_debug(
		"TWAIN init: " TWAIN_DSM_DLL "->GetProcAddress("
		TWAIN_DSM_ENTRY_FN_NAME ") successful"
	);

	private->init = 1;
	return LIS_OK;
}


static void twain_cleanup(struct lis_api *impl)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);

	if (private->init) {
		FreeLibrary(private->twain_dll);
		private->init = 0;
	}
	FREE(private);
}

static enum lis_error twain_list_devices(
		struct lis_api *impl, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);
	enum lis_error err;

	LIS_UNUSED(locs);
	LIS_UNUSED(dev_infos);

	err = twain_init(private);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static enum lis_error twain_get_device(
		struct lis_api *impl, const char *dev_id,
		struct lis_item **item
	)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);
	enum lis_error err;

	LIS_UNUSED(dev_id);
	LIS_UNUSED(item);

	err = twain_init(private);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


enum lis_error lis_api_twain(struct lis_api **impl)
{
	// Reminder: this is the only function not protected
	// by the thread-safe normalizer
	// --> keep things to a minimum here

	struct lis_twain_private *private;

	private = calloc(1, sizeof(struct lis_twain_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	memcpy(&private->parent, &g_twain_template, sizeof(private->parent));

	*impl = &private->parent;
	return LIS_OK;
}
