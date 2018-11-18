#include <stdbool.h>
#include <stdio.h>
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

#define MAX_ID_LENGTH 64 // is manufacturer name + model. TWAIN limits both to 32


struct lis_twain_dev_desc {
	struct lis_device_descriptor parent;
	TW_IDENTITY twain_id;

	struct lis_twain_dev_desc *next;
};


struct lis_twain_private {
	struct lis_api parent;

	bool init;

	struct lis_twain_dev_desc *devices;
	struct lis_device_descriptor **device_ptrs;
};
#define LIS_TWAIN_PRIVATE(impl) ((struct lis_twain_private *)(impl))


struct lis_twain_item {
	struct lis_item parent;

	TW_IDENTITY twain_id;
};
#define LIS_TWAIN_ITEM_PRIVATE(item) ((struct lis_twain_item *)(item))


static const TW_IDENTITY g_libinsane_identity_template = {
	.Id = 0,
	.Version = {
		.MajorNum = 1,
		.MinorNum = 0,
		.Language = TWLG_USA,
		.Country = TWCY_USA,
		.Info = "1.0.0",
	},
	.ProtocolMajor = TWON_PROTOCOLMAJOR,
	.ProtocolMinor = TWON_PROTOCOLMINOR,
	.SupportedGroups = DF_APP2 | DG_IMAGE | DG_CONTROL,
	.Manufacturer = "OpenPaper.work",
	.ProductFamily = "Libinsane",
	.ProductName = "Libinsane",
};


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


static enum lis_error twain_get_children(
	struct lis_item *self, struct lis_item ***children
);
static enum lis_error twain_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error twain_scan_start(
	struct lis_item *self, struct lis_scan_session **session
);
static void twain_close(struct lis_item *self);


static struct lis_item g_twain_item_template = {
	.get_children = twain_get_children,
	.get_options = twain_get_options,
	.scan_start = twain_scan_start,
	.close = twain_close,
};


static int g_init = 0;

static TW_IDENTITY g_app_id;
static HMODULE g_twain_dll;
static DSMENTRYPROC g_dsm_entry_fn;


static enum lis_error twrc_to_lis_error(TW_UINT16 twrc)
{
	const char *name;

	switch(twrc & (~TWRC_CUSTOMBASE)) {
		case TWRC_SUCCESS:
			return LIS_OK;
		case TWRC_FAILURE:
			name = "TWRC_FAILURE";
			break;
		case TWRC_CHECKSTATUS:
			name = "TWRC_CHECKSTATUS";
			break;
		case TWRC_CANCEL:
			return LIS_ERR_CANCELLED;
		case TWRC_DSEVENT:
			name = "TWRC_DSEVENT";
			break;
		case TWRC_NOTDSEVENT:
			name = "TWRC_NOTDSEVENT";
			break;
		case TWRC_XFERDONE:
			name = "TWRC_XFERDONE";
			break;
		case TWRC_ENDOFLIST:
			name = "TWRC_ENDOFLIST";
			break;
		case TWRC_INFONOTSUPPORTED:
			// TODO(Jflesch): need to be checked
			return LIS_ERR_INVALID_VALUE;
		case TWRC_DATANOTAVAILABLE:
			// TODO(Jflesch): need to be checked
			return LIS_ERR_IO_ERROR;
		case TWRC_BUSY:
			return LIS_ERR_DEVICE_BUSY;
		case TWRC_SCANNERLOCKED:
			return LIS_ERR_HW_IS_LOCKED;
		default:
			name = "N/A";
			break;
	}

	lis_log_warning("Unknown/unexpected error: 0x%X (%s)", twrc, name);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}


static void check_twain_status(struct lis_twain_private *private)
{
	TW_UINT16 twrc;
	TW_STATUS twain_status;
	enum lis_error err;
	const char *condition_code_str;

	LIS_UNUSED(private);

	twrc = g_dsm_entry_fn(
		&g_app_id, NULL,
		DG_CONTROL, DAT_STATUS, MSG_GET,
		&twain_status
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to get TWAIN status: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		return;
	}

	switch(twain_status.ConditionCode) {
#define CC_TO_STR(cc) case cc: condition_code_str = #cc ; break
		CC_TO_STR(TWCC_SUCCESS);
		CC_TO_STR(TWCC_BUMMER);
		CC_TO_STR(TWCC_LOWMEMORY);
		CC_TO_STR(TWCC_NODS);
		CC_TO_STR(TWCC_MAXCONNECTIONS);
		CC_TO_STR(TWCC_OPERATIONERROR);
		CC_TO_STR(TWCC_BADCAP);
		CC_TO_STR(TWCC_BADPROTOCOL);
		CC_TO_STR(TWCC_BADVALUE);
		CC_TO_STR(TWCC_SEQERROR);
		CC_TO_STR(TWCC_BADDEST);
		CC_TO_STR(TWCC_CAPUNSUPPORTED);
		CC_TO_STR(TWCC_CAPBADOPERATION);
		CC_TO_STR(TWCC_CAPSEQERROR);
		CC_TO_STR(TWCC_DENIED);
		CC_TO_STR(TWCC_FILEEXISTS);
		CC_TO_STR(TWCC_FILENOTFOUND);
		CC_TO_STR(TWCC_NOTEMPTY);
		CC_TO_STR(TWCC_PAPERJAM);
		CC_TO_STR(TWCC_PAPERDOUBLEFEED);
		CC_TO_STR(TWCC_FILEWRITEERROR);
		CC_TO_STR(TWCC_CHECKDEVICEONLINE);
		CC_TO_STR(TWCC_INTERLOCK);
		CC_TO_STR(TWCC_DAMAGEDCORNER);
		CC_TO_STR(TWCC_FOCUSERROR);
		CC_TO_STR(TWCC_DOCTOOLIGHT);
		CC_TO_STR(TWCC_DOCTOODARK);
		CC_TO_STR(TWCC_NOMEDIA);
#undef CC_TO_STR
		default:
			condition_code_str = "unknown";
			break;
	}

	lis_log_error(
		"TWAIN status: Condition code = %s (0x%X) ; data = 0x%X",
		condition_code_str, twain_status.ConditionCode,
		twain_status.Data
	);
}


static enum lis_error twain_init(struct lis_twain_private *private)
{
	TW_UINT16 twrc;
	enum lis_error err;

	if (private->init) {
		return LIS_OK;
	}

	if (g_init > 0) {
		private->init = 1;
		g_init++;
		return LIS_OK;
	}

	lis_log_debug("TWAIN init: LoadLibrary(" TWAIN_DSM_DLL ")");
	g_twain_dll = LoadLibraryA(TWAIN_DSM_DLL);
	if (g_twain_dll == NULL) {
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
	// XXX(Jflesch):
	// GetProcAddress returns a FARPROC, and we need to cast it
	// to a DSMENTRYPROC. With MSYS2 + GCC 8.2 (64bits), it raises
	// the warning "error: cast between incompatible function types".
	// So we have to do a double-cast here.
	g_dsm_entry_fn = (DSMENTRYPROC)(void (*)(void))GetProcAddress(
		g_twain_dll, TWAIN_DSM_ENTRY_FN_NAME
	);
	if (g_dsm_entry_fn == NULL) {
		lis_log_error(
			"Failed to load Twain DSM_Entry function: 0x%lX",
			GetLastError()
		);
		FreeLibrary(g_twain_dll);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}
	lis_log_debug(
		"TWAIN init: " TWAIN_DSM_DLL "->GetProcAddress("
		TWAIN_DSM_ENTRY_FN_NAME ") successful"
	);

	lis_log_debug(
		"TWAIN init: DsmEntry(DG_CONTROL, DAT_PARENT, MSG_OPENDSM)"
	);
	memcpy(
		&g_app_id, &g_libinsane_identity_template,
		sizeof(g_app_id)
	);
	twrc = g_dsm_entry_fn(
		&g_app_id, NULL,
		DG_CONTROL, DAT_PARENT, MSG_OPENDSM, NULL
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to open DSM: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		check_twain_status(private);
		return err;
	}
	lis_log_debug(
		"TWAIN init: DsmEntry(DG_CONTROL, DAT_PARENT, MSG_OPENDSM)"
		" successful"
	);

	private->init = TRUE;
	return LIS_OK;
}


static void free_devices(struct lis_twain_private *private)
{
	struct lis_twain_dev_desc *dev, *ndev;

	for (dev = private->devices, ndev = (dev != NULL) ? dev->next : NULL ;
			dev != NULL ;
			dev = ndev, ndev = (ndev != NULL) ? ndev->next : NULL) {
		FREE(dev);
	}
	private->devices = NULL;
	FREE(private->device_ptrs);
}


static void twain_cleanup(struct lis_api *impl)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);

	if (private->init) {
		private->init = FALSE;
		g_init--;
		if (g_init <= 0) {
			FreeLibrary(g_twain_dll);
		}
	}
	free_devices(private);
	FREE(private);
}


static void make_device_id(char *out, TW_IDENTITY *twain_id)
{
	snprintf(
		out, MAX_ID_LENGTH + 1, "%s:%s",
		twain_id->Manufacturer, twain_id->ProductName
	);
}


static enum lis_error twain_list_devices(
		struct lis_api *impl, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);
	enum lis_error err;
	int src_idx;
	TW_UINT16 twrc;
	struct lis_twain_dev_desc *dev;

	LIS_UNUSED(locs);

	lis_log_info("TWAIN->list_devices()");
	err = twain_init(private);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	free_devices(private);

	for (src_idx = 0 ; ; src_idx++) {
		dev = calloc(1, sizeof(struct lis_twain_dev_desc));
		if (dev == NULL) {
			lis_log_error("Out of memory");
			free_devices(private);
			return LIS_ERR_NO_MEM;
		}

		lis_log_debug("DsmEntry(DG_CONTROL, DAT_IDENTITY)");
		twrc = g_dsm_entry_fn(
			&g_app_id, NULL,
			DG_CONTROL, DAT_IDENTITY,
			(src_idx == 0) ? MSG_GETFIRST : MSG_GETNEXT,
			&dev->twain_id
		);
		if (twrc == TWRC_ENDOFLIST) {
			FREE(dev);
			lis_log_debug(
				"DsmEntry(DG_CONTROL, DAT_IDENTITY):"
				" end of list"
			);
			break;
		}
		if (twrc != TWRC_SUCCESS) {
			err = twrc_to_lis_error(twrc);
			lis_log_error(
				"DsmEntry(DG_CONTROL, DAT_IDENTITY) failed:"
				" 0x%X -> 0x%X, %s", twrc, err,
				lis_strerror(err)
			);
			check_twain_status(private);
			FREE(dev);
			free_devices(private);
			return err;
		}

		lis_log_info(
			"DsmEntry(DG_CONTROL, DAT_IDENTITY):"
			" Found device: %ld, %s %s (%s)",
			dev->twain_id.Id,
			dev->twain_id.Manufacturer,
			dev->twain_id.ProductName,
			dev->twain_id.ProductFamily
		);

		dev->parent.dev_id = calloc(MAX_ID_LENGTH + 1, sizeof(char));
		if (dev->parent.dev_id == NULL) {
			lis_log_error("Out of memory");
			FREE(dev);
			free_devices(private);
			return LIS_ERR_NO_MEM;
		}
		make_device_id(dev->parent.dev_id, &dev->twain_id);

		dev->parent.vendor = dev->twain_id.Manufacturer;
		dev->parent.model = dev->twain_id.ProductName;
		// TODO(Jflesch): Find type
		dev->parent.type = "unknown";

		dev->next = private->devices;
		private->devices = dev;
	}

	lis_log_info("TWAIN->list_devices(): %d devices found", src_idx);

	private->device_ptrs = calloc(
		src_idx + 1, sizeof(struct lis_device_descriptor *)
	);
	if (dev_infos != NULL) {
		(*dev_infos) = private->device_ptrs;
	}
	if (private->device_ptrs == NULL) {
		lis_log_error("Out of memory");
		free_devices(private);
		return LIS_ERR_NO_MEM;
	}

	for (dev = private->devices, src_idx = 0 ;
			dev != NULL ;
			dev = dev->next, src_idx++) {
		private->device_ptrs[src_idx] = &dev->parent;
	}

	// TODO(Jflesch): should only report devices that are actually
	// online.
	return LIS_OK;
}


static void twain_close(struct lis_item *self)
{
	FREE(self->name);
	FREE(self);
}


static enum lis_error twain_get_device(
		struct lis_api *impl, const char *dev_id,
		struct lis_item **out_item
	)
{
	struct lis_twain_private *private = LIS_TWAIN_PRIVATE(impl);
	enum lis_error err;
	struct lis_twain_dev_desc *dev;
	struct lis_twain_item *item;
	char other_dev_id[MAX_ID_LENGTH + 1];

	lis_log_info("TWAIN->get_device(%s)", dev_id);
	err = twain_init(private);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (private->devices == NULL) {
		lis_log_debug("No device list loaded yet. Loading now");
		err = twain_list_devices(
			impl, LIS_DEVICE_LOCATIONS_ANY, NULL
		);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
	}

	for (dev = private->devices ; dev != NULL ; dev = dev->next) {
		make_device_id(other_dev_id, &dev->twain_id);
		if (strcasecmp(other_dev_id, dev_id) == 0) {
			break;
		}
	}

	if (dev == NULL) {
		lis_log_error(
			"TWAIN->get_device(%s): Device not found", dev_id
		);
		return LIS_ERR_INVALID_VALUE;
	}

	item = calloc(1, sizeof(struct lis_twain_item));
	if (item == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	memcpy(&item->parent, &g_twain_item_template, sizeof(item->parent));
	memcpy(&item->twain_id, &dev->twain_id, sizeof(item->twain_id));
	item->parent.name = strdup(dev_id);
	item->parent.type = LIS_ITEM_UNIDENTIFIED; // TODO(Jflesch)

	*out_item = &item->parent;

	return LIS_OK;
}


static enum lis_error twain_get_children(
		struct lis_item *self, struct lis_item ***children
	)
{
	static struct lis_item *twain_children[] = { NULL };

	LIS_UNUSED(self);

	*children = twain_children;
	return LIS_OK;
}

static enum lis_error twain_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	LIS_UNUSED(self);
	LIS_UNUSED(descs);

	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static enum lis_error twain_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	)
{
	LIS_UNUSED(self);
	LIS_UNUSED(session);

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
