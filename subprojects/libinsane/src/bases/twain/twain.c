#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <libinsane/capi.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/twain.h>
#include <libinsane/util.h>

#include "../../bmp.h"
#include "capabilities.h"
#include "twain.h"

#define NAME "twain"
#define TWAIN_DSM_DLL "twaindsm.dll"
#define TWAIN_DSM_ENTRY_FN_NAME "DSM_Entry"

#define MAX_ID_LENGTH 64 // is manufacturer name + model. TWAIN limits both to 32
#define MAX_MSG 16


struct lis_twain_dev_desc {
	struct lis_device_descriptor parent;
	TW_IDENTITY twain_id;

	struct lis_twain_dev_desc *next;
};


struct lis_twain_private {
	struct lis_api parent;

	bool init;

	TW_ENTRYPOINT entry_points;

	struct lis_twain_dev_desc *devices;
	struct lis_device_descriptor **device_ptrs;
};
#define LIS_TWAIN_PRIVATE(impl) ((struct lis_twain_private *)(impl))


struct lis_twain_option {
	struct lis_option_descriptor parent;

	const struct lis_twain_cap *twain_cap;
	struct lis_twain_item *item;
};
#define LIS_TWAIN_OPT_PRIVATE(opt) ((struct lis_twain_option *)(opt))


struct lis_twain_item {
	struct lis_item parent;
	struct lis_twain_private *impl;

	TW_IDENTITY twain_id;
	bool use_callback;

	struct lis_twain_option *opts;
	struct lis_option_descriptor **opts_ptrs;

	struct lis_twain_session *session;

	struct {
		TW_UINT16 msgs[MAX_MSG];
		int write_idx;
		int read_idx;

		/*!
		 * DSM callbacks and application thread use this semaphore
		 * to synchronize.
		 */
		HANDLE sem;
	} dsm2app;

	struct lis_twain_item *next;
};
#define LIS_TWAIN_ITEM_PRIVATE(item) ((struct lis_twain_item *)(item))


struct lis_twain_session {
	struct lis_scan_session parent;

	struct lis_twain_item *item;

	bool cancelled;
	bool xfer_pending;

	struct {
		TW_IMAGEINFO infos;
		TW_HANDLE handle;
		void *mem;

		struct bmp_header header;

		int header_size;
		int header_read;
		int img_size;
		int img_read;
	} img;
};
#define LIS_TWAIN_SESSION_PRIVATE(item) ((struct lis_twain_session *)(item))


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


static enum lis_error twain_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *parameters
);
static int twain_end_of_feed(struct lis_scan_session *session);
static int twain_end_of_page(struct lis_scan_session *session);
static enum lis_error twain_scan_read(
	struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
);
static void twain_cancel(struct lis_scan_session *session);


static struct lis_scan_session g_twain_session_template = {
	.get_scan_parameters = twain_get_scan_parameters,
	.end_of_feed = twain_end_of_feed,
	.end_of_page = twain_end_of_page,
	.scan_read = twain_scan_read,
	.cancel = twain_cancel,
};


static int g_init = 0;

static TW_IDENTITY g_app_id;
static HMODULE g_twain_dll;
static DSMENTRYPROC g_dsm_entry_fn;

/* TWAIN callback is really annoying. It only provides TW_IDENTITY
 * to find back the item it's talking about (no user pointer).
 * Which mean we have to keep a global list of all active items ...
 */
static struct lis_twain_item *g_items = NULL;


static inline double twain_unfix(const TW_FIX32 *fix32)
{
	return ((double)fix32->Whole) + (((double)fix32->Frac) / 65536.0);
}


static inline TW_FIX32 twain_fix(double val)
{
	TW_FIX32 fix32;

	fix32.Whole = val;
	val -= fix32.Whole;
	fix32.Frac = (val * 65536.0);

	return fix32;
}


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


static void check_twain_status(
		const char *file, int line, const char *func,
		enum lis_log_level level
	)
{
	TW_UINT16 twrc;
	TW_STATUS twain_status;
	enum lis_error err;
	const char *condition_code_str;

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

	lis_log(
		level, file, line, func,
		"TWAIN status: Condition code = %s (0x%X) ; data = 0x%X",
		condition_code_str, twain_status.ConditionCode,
		twain_status.Data
	);
}


#define DSM_ENTRY(dest, dg, dat, msg, data) \
	_dsm_entry( \
		__FILE__, __LINE__, __func__, \
		dest, #dest, \
		dg, #dg, \
		dat, #dat, \
		msg, #msg, \
		data \
	)


static TW_UINT16 _dsm_entry(
		const char *file, int line, const char *func,
		TW_IDENTITY *dest, const char *dest_str,
		TW_UINT32 dg, const char *dg_str,
		TW_UINT16 dat, const char *dat_str,
		TW_UINT16 msg, const char *msg_str,
		void *data
	)
{
	enum lis_error err;
	TW_UINT16 twrc;
	enum lis_log_level level;
	int check_status;
	int error;

	lis_log(
		LIS_LOG_LVL_DEBUG, file, line, func,
		"TWAIN->DSM_Entry(%s, %s, %s, %s, 0x%p) ...",
		dest_str, dg_str, dat_str, msg_str, data
	);
	twrc = g_dsm_entry_fn(&g_app_id, dest, dg, dat, msg, data);
	switch(twrc) {
		case TWRC_SUCCESS:
		case TWRC_ENDOFLIST:
		case TWRC_XFERDONE:
			level = LIS_LOG_LVL_DEBUG;
			error = 0;
			check_status = 0;
			break;
		case TWRC_CHECKSTATUS:
			level = LIS_LOG_LVL_INFO;
			error = 0;
			check_status = 1;
			break;
		default:
			level = LIS_LOG_LVL_WARNING;
			error = 1;
			check_status = 1;
			break;
	}

	if (error) {
		err = twrc_to_lis_error(twrc);
		lis_log(
			level, file, line, func,
			"TWAIN->DSM_Entry(%s, %s, %s, %s, 0x%p) failed:"
			" 0x%X -> 0x%X, %s",
			dest_str, dg_str, dat_str, msg_str, data,
			twrc, err, lis_strerror(err)
		);
	} else {
		lis_log(
			level, file, line, func,
			"TWAIN->DSM_Entry(%s, %s, %s, %s, 0x%p): 0x%X",
			dest_str, dg_str, dat_str, msg_str, data, twrc
		);
	}
	if (check_status) {
		check_twain_status(file, line, func, level);
	}
	return twrc;
}


static enum lis_error twain_init(struct lis_twain_private *private)
{
	TW_UINT16 twrc;
	enum lis_error err;
	HANDLE hnd;

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

	memcpy(
		&g_app_id, &g_libinsane_identity_template,
		sizeof(g_app_id)
	);
	hnd = GetConsoleWindow();
	if (hnd == NULL) {
		hnd = GetDesktopWindow();
	}
	twrc = DSM_ENTRY(NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, NULL);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to open DSM: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		return err;
	}

	if(g_app_id.SupportedGroups & DF_DSM2) {
		private->entry_points.Size = sizeof(private->entry_points);
		twrc = DSM_ENTRY(
			NULL, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET,
			&private->entry_points
		);
		if (twrc != TWRC_SUCCESS) {
			err = twrc_to_lis_error(twrc);
			lis_log_error(
				"Failed to get TWAIN entry points:"
				" 0x%X -> 0x%X, %s",
				twrc, err, lis_strerror(err)
			);
			return err;
		}
	}

	lis_log_info("TWAIN DSM init successful");
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

		twrc = DSM_ENTRY(
			NULL, DG_CONTROL, DAT_IDENTITY,
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
		dev->parent.type = "unknown"; // TODO(Jflesch)

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
	struct lis_twain_item *private = LIS_TWAIN_ITEM_PRIVATE(self);
	struct lis_twain_item *item, *pitem;

	// unregister item
	for (pitem = NULL, item = g_items ;
			item != NULL ;
			pitem = item, item = item->next) {
		if (item == private) {
			if (pitem == NULL) {
				g_items = item->next;
			} else {
				pitem->next = item->next;
			}
		}
	}

	DSM_ENTRY(
		NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS,
		&private->twain_id
	);
	FREE(self->name);
	FREE(self);
}


static TW_UINT16 twain_dsm_callback(
		TW_IDENTITY *origin, TW_IDENTITY *dest,
		TW_UINT32 dg, TW_UINT16 dat, TW_UINT16 msg,
		void *data
	)
{
	struct lis_twain_item *private;

	LIS_UNUSED(dest);

	for (private = g_items ; private != NULL ; private = private->next) {
		if (private->twain_id.Id == origin->Id) {
			break;
		}
	}

	if (private == NULL) {
		lis_log_warning(
			"DSM Callback(0x%lX, 0x%X, 0x%X, 0x%p) called for"
			" unknown source id: %ld",
			dg, dat, msg, data, origin->Id
		);
		return TWRC_FAILURE;
	}

	lis_log_info(
		"DSM Callback(%s, 0x%lX, 0x%X, 0x%X, 0x%p) called by DSM",
		private->parent.name, dg, dat, msg, data
	);

	if (private->dsm2app.sem == NULL) {
		lis_log_warning(
			"DSM Callback(%s, 0x%lX, 0x%X, 0x%X, 0x%p) called"
			" when not expected",
			private->parent.name, dg, dat, msg, data
		);
		return TWRC_FAILURE;
	}

	private->dsm2app.msgs[private->dsm2app.write_idx] = msg;
	private->dsm2app.write_idx++;
	private->dsm2app.write_idx %= MAX_MSG;

	ReleaseSemaphore(
		private->dsm2app.sem, 1 /* count */,
		NULL /* previous count */
	);

	return TWRC_SUCCESS;
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
	TW_UINT16 twrc;
	DSMENTRYPROC cb = twain_dsm_callback;
	TW_CALLBACK callback = {
		.CallBackProc = *((void **)(&cb)),
		.RefCon = 0,
		.Message = 0,
	};

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

	item->impl = private;
	memcpy(&item->parent, &g_twain_item_template, sizeof(item->parent));
	memcpy(&item->twain_id, &dev->twain_id, sizeof(item->twain_id));
	item->parent.name = strdup(dev_id);
	item->parent.type = LIS_ITEM_UNIDENTIFIED; // TODO(Jflesch)

	twrc = DSM_ENTRY(
		NULL, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS,
		&item->twain_id
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to open datasource '%s':"
			" 0x%X -> 0x%X, %s",
			dev_id, twrc, err, lis_strerror(err)
		);
		FREE(item);
		return err;
	}

	// register item
	item->next = g_items;
	g_items = item;

	item->use_callback = (
		(g_app_id.SupportedGroups & DF_DSM2)
			&& (item->twain_id.SupportedGroups & DF_DS2)
	);
	lis_log_info("Twain source: use callback = %d", item->use_callback);

	if (item->use_callback) {
		twrc = DSM_ENTRY(
			&item->twain_id, DG_CONTROL, DAT_CALLBACK,
			MSG_REGISTER_CALLBACK, &callback
		);
		if (twrc != TWRC_SUCCESS) {
			err = twrc_to_lis_error(twrc);
			lis_log_error(
				"Failed to register callback for datasource"
				" '%s': 0x%X -> 0x%X, %s",
				dev_id, twrc, err, lis_strerror(err)
			);
			twain_close(&item->parent);
			return err;
		}
	}

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


static enum lis_value_type twain_type_to_lis(
		const char *opt_name, TW_UINT16 twain_type
	)
{
	switch(twain_type) {
		case TWTY_INT8:
		case TWTY_INT16:
		case TWTY_INT32:
		case TWTY_UINT8:
		case TWTY_UINT16:
		case TWTY_UINT32:
			return LIS_TYPE_INTEGER;
		case TWTY_BOOL:
			return LIS_TYPE_BOOL;
		case TWTY_FIX32:
			return LIS_TYPE_DOUBLE;
		case TWTY_STR32:
		case TWTY_STR64:
		case TWTY_STR128:
		case TWTY_STR255:
			return LIS_TYPE_STRING;
		case TWTY_FRAME:
		case TWTY_HANDLE:
		default:
			break;
	}

	lis_log_warning(
		"Unknown twain type for option '%s': 0x%X."
		" Assuming integer", opt_name, twain_type
	);
	return LIS_TYPE_INTEGER;
}


static enum lis_value_type twain_cap_to_lis_type(
		const TW_CAPABILITY *cap,
		const struct lis_twain_cap *twain_cap_def,
		const void *twain_container
	)
{
	union {
		const TW_ARRAY *array;
		const TW_ENUMERATION *enumeration;
		const TW_ONEVALUE *one;
		const TW_RANGE *range;
	} container;
	enum lis_value_type type;
	int valid = 0;

	switch(cap->ConType) {
		case TWON_ARRAY:
			container.array = twain_container;
			type = twain_type_to_lis(
				twain_cap_def->name, container.array->ItemType
			);
			valid = 1;
			break;
		case TWON_ENUMERATION:
			container.enumeration = twain_container;
			type = twain_type_to_lis(
				twain_cap_def->name,
				container.enumeration->ItemType
			);
			valid = 1;
			break;
		case TWON_ONEVALUE:
			container.one = twain_container;
			type = twain_type_to_lis(
				twain_cap_def->name, container.one->ItemType
			);
			valid = 1;
			break;
		case TWON_RANGE:
			container.range = twain_container;
			type = twain_type_to_lis(
				twain_cap_def->name, container.range->ItemType
			);
			valid = 1;
			break;
	}

	if (!valid) {
		lis_log_warning(
			"Unknown twain container for option '%s': 0x%X."
			" Assuming integer", twain_cap_def->name, cap->ConType
		);
		return LIS_TYPE_INTEGER;
	}

	if (type == LIS_TYPE_INTEGER && twain_cap_def->possibles != NULL) {
		return LIS_TYPE_STRING;
	}

	return type;
}


static int get_twain_type_size(TW_UINT16 type)
{
	switch(type) {
		case TWTY_INT8:
			return sizeof(TW_INT8);
		case TWTY_INT16:
			return sizeof(TW_INT16);
		case TWTY_INT32:
			return sizeof(TW_INT32);
		case TWTY_UINT8:
			return sizeof(TW_UINT8);
		case TWTY_UINT16:
			return sizeof(TW_UINT16);
		case TWTY_UINT32:
			return sizeof(TW_UINT32);
		case TWTY_BOOL:
			return sizeof(TW_BOOL);
		case TWTY_FIX32:
			return sizeof(TW_FIX32);
		case TWTY_STR32:
			return sizeof(TW_STR32);
		case TWTY_STR64:
			return sizeof(TW_STR64);
		case TWTY_STR128:
			return sizeof(TW_STR128);
		case TWTY_STR255:
			return sizeof(TW_STR255);
		case TWTY_FRAME:
			return sizeof(TW_FRAME);
		case TWTY_HANDLE:
			return sizeof(TW_HANDLE);
		default:
			break;
	}

	lis_log_warning("Unknown TWAIN type 0x%X. Assuming size = 1", type);
	return 1;
}


static enum lis_error twain_int_to_str(
		int in_integer, const struct lis_twain_cap *twain_cap_def,
		union lis_value *out
	)
{
	int i;

	out->integer = in_integer;
	if (twain_cap_def->possibles == NULL) {
		return LIS_OK;
	}

	for (i = 0 ; !twain_cap_def->possibles[i].eol ; i++) {
		if (twain_cap_def->possibles[i].twain_int == in_integer) {
			out->string = twain_cap_def->possibles[i].str;
			return LIS_OK;
		}
	}

	out->string = "unknown";
	return LIS_OK; // not really but not much we can do ...
}


static enum lis_error twain_cap_to_lis(
		TW_UINT16 type, const struct lis_twain_cap *twain_cap_def,
		const void *in, union lis_value *out
	)
{
	union {
		const TW_INT8 *int8;
		const TW_INT16 *int16;
		const TW_INT32 *int32;
		const TW_UINT8 *uint8;
		const TW_UINT16 *uint16;
		const TW_UINT32 *uint32;
		const TW_BOOL *boolean;
		const TW_FIX32 *fix32;
		const char *str;
	} value;

	switch(type) {
		case TWTY_INT8:
			value.int8 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_INT16:
			value.int16 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_INT32:
			value.int32 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_UINT8:
			value.uint8 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_UINT16:
			value.uint16 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_UINT32:
			value.uint32 = in;
			twain_int_to_str(*(value.int8), twain_cap_def, out);
			return LIS_OK;
		case TWTY_BOOL:
			value.boolean = in;
			out->boolean = *(value.boolean);
			return LIS_OK;
		case TWTY_FIX32:
			value.fix32 = in;
			out->dbl = twain_unfix(value.fix32);
			return LIS_OK;
		case TWTY_STR32:
		case TWTY_STR64:
		case TWTY_STR128:
		case TWTY_STR255:
			value.str = in;
			out->string = strdup(value.str);
			// TODO(Jflesch): check strdup() result
			return LIS_OK;
	}

	lis_log_warning(
		"Unknown twain type: 0x%X. Can't convert", (int)type
	);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static enum lis_error get_constraint(
		struct lis_option_descriptor *opt,
		const struct lis_twain_cap *twain_cap_def,
		const TW_CAPABILITY *cap,
		const void *twain_container
	)
{
	union {
		const TW_ARRAY *array;
		const TW_ENUMERATION *enumeration;
		const TW_ONEVALUE *one;
		const TW_RANGE *range;
	} container;
	size_t el_size;
	unsigned int i;

	if (opt->value.type == LIS_TYPE_BOOL) {
		opt->constraint.type = LIS_CONSTRAINT_NONE;
		return LIS_OK;
	}

	switch(cap->ConType) {
		case TWON_ARRAY:
			container.array = twain_container;
			opt->constraint.type = LIS_CONSTRAINT_LIST;
			opt->constraint.possible.list.nb_values =
				container.array->NumItems;
			opt->constraint.possible.list.values =
				calloc(
					container.array->NumItems,
					sizeof(union lis_value)
				);
			// TODO: check calloc
			el_size = get_twain_type_size(
				container.array->ItemType
			);
			for (i = 0 ; i < container.array->NumItems ; i++) {
				twain_cap_to_lis(
					container.array->ItemType, twain_cap_def,
					((char *)(&container.array->ItemList)) + (i * el_size),
					&(opt->constraint.possible.list.values[i])
				);
				// TODO: check twain_value_to_lis()
			}
			return LIS_OK;
		case TWON_ENUMERATION:
			container.enumeration = twain_container;
			opt->constraint.type = LIS_CONSTRAINT_LIST;
			opt->constraint.possible.list.nb_values =
				container.enumeration->NumItems;
			opt->constraint.possible.list.values =
				calloc(
					container.enumeration->NumItems,
					sizeof(union lis_value)
				);
			// TODO: check calloc
			el_size = get_twain_type_size(
				container.enumeration->ItemType
			);
			for (i = 0 ; i < container.enumeration->NumItems ; i++) {
				twain_cap_to_lis(
					container.enumeration->ItemType, twain_cap_def,
					((char *)(&container.enumeration->ItemList)) + (i * el_size),
					&(opt->constraint.possible.list.values[i])
				);
				// TODO: check twain_value_to_lis()
			}
			return LIS_OK;
		case TWON_ONEVALUE:
			container.one = twain_container;
			opt->constraint.type = LIS_CONSTRAINT_LIST;
			opt->constraint.possible.list.nb_values = 1;
			opt->constraint.possible.list.values =
				calloc(1, sizeof(union lis_value));
			// TODO: check calloc
			twain_cap_to_lis(
				container.one->ItemType,
				twain_cap_def,
				&container.one->Item,
				&opt->constraint.possible.list.values[0]
			);
			// TODO: check twain_value_to_lis()
			return LIS_OK;
		case TWON_RANGE:
			container.range = twain_container;
			opt->constraint.type = LIS_CONSTRAINT_RANGE;
			if (container.range->ItemType == TWTY_FIX32) {
				opt->constraint.possible.range.min.dbl =
					twain_unfix(
						(TW_FIX32 *)
						(&container.range->MinValue)
					);
				opt->constraint.possible.range.max.dbl  =
					twain_unfix(
						(TW_FIX32 *)
						(&container.range->MaxValue)
					);
				opt->constraint.possible.range.interval.dbl =
					twain_unfix(
						(TW_FIX32 *)
						(&container.range->StepSize)
					);
			} else {
				opt->constraint.possible.range.min.integer =
					container.range->MinValue;
				opt->constraint.possible.range.max.integer =
					container.range->MaxValue;
				opt->constraint.possible.range.interval.integer =
					container.range->StepSize;
			}
			return LIS_OK;
	}

	opt->constraint.type = LIS_CONSTRAINT_NONE;
	lis_log_warning(
		"Unknown twain container: 0x%X. Assuming integer", cap->ConType
	);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}



static enum lis_error twain_simple_get_value(
		struct lis_option_descriptor *self, union lis_value *out
	)
{
	struct lis_twain_option *private = LIS_TWAIN_OPT_PRIVATE(self);
	TW_UINT16 twrc;
	TW_CAPABILITY cap;
	enum lis_error err;
	const TW_ONEVALUE *container;

	lis_log_info(
		"%s->simple_get_value(%s) ...",
		private->item->parent.name, self->name
	);
	memset(&cap, 0, sizeof(cap));
	cap.Cap = private->twain_cap->id;
	cap.ConType = TWON_DONTCARE16;
	twrc = DSM_ENTRY(
		&private->item->twain_id,
		DG_CONTROL, DAT_CAPABILITY, MSG_GETCURRENT,
		&cap
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"%s->simple_get_value(%s): Failed to get value: 0x%X, %s",
			private->item->parent.name, self->name,
			err, lis_strerror(err)
		);
		return err;
	}

	if (cap.ConType != TWON_ONEVALUE) {
		lis_log_error(
			"%s->simple_get_value(%s): Unsupported container type: 0x%X",
			private->item->parent.name, self->name,
			cap.ConType
		);
		return LIS_ERR_UNSUPPORTED;
	}

	container = private->item->impl->entry_points.DSM_MemLock(
		cap.hContainer
	);

	err = twain_cap_to_lis(
		container->ItemType, private->twain_cap,
		&container->Item, out
	);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s->simple_get_value(%s): Failed to convert value: 0x%x, %s",
			private->item->parent.name, self->name,
			err, lis_strerror(err)
		);
		goto end;
	}
	lis_log_info(
		"%s->simple_get_value(%s) successful",
		private->item->parent.name, self->name
	);

end:
	private->item->impl->entry_points.DSM_MemUnlock(cap.hContainer);
	private->item->impl->entry_points.DSM_MemFree(cap.hContainer);
	return err;
}


static enum lis_error lis_str_to_twain_int(
		const struct lis_twain_cap *cap, const char *str,
		int *out
	)
{
	int i;

	*out = 0;

	for (i = 0 ; !cap->possibles[i].eol ; i++) {
		if (strcasecmp(str, cap->possibles[i].str) == 0) {
			*out = cap->possibles[i].twain_int;
			return LIS_OK;
		}
	}

	return LIS_ERR_INVALID_VALUE;
}


/* TW_ONEVALUE is so poorly defined ... */
#pragma pack (push, before_twain)
#pragma pack (2)
union lis_one_value {
	TW_ONEVALUE twain;
	struct {
		TW_UINT16 item_type;
		union {
			TW_BOOL boolean;
			TW_INT8 integer8;
			TW_INT16 integer16;
			TW_INT32 integer32;
			TW_FIX32 dbl;
			TW_STR255 str;
		} value;
	} lis;
};
#pragma pack (pop, before_twain)


static enum lis_error twain_simple_set_value(
		struct lis_option_descriptor *self, union lis_value in_value,
		int *set_flags
	)
{
	struct lis_twain_option *private = LIS_TWAIN_OPT_PRIVATE(self);
	TW_UINT16 twrc;
	TW_CAPABILITY cap;
	enum lis_error err = LIS_OK;
	union lis_one_value *value;
	int intvalue = 0;

	lis_log_info(
		"%s->simple_set_value(%s) ...",
		private->item->parent.name, self->name
	);

	// always assume result is inexact (we don't get any feedback)
	*set_flags = LIS_SET_FLAG_INEXACT;

	memset(&cap, 0, sizeof(cap));
	cap.Cap = private->twain_cap->id;
	cap.ConType = TWON_ONEVALUE;

	cap.hContainer = private->item->impl->entry_points.DSM_MemAllocate(
		sizeof(union lis_one_value)
	);
	// TODO(Jflesch): out of mem ?

	value = private->item->impl->entry_points.DSM_MemLock(cap.hContainer);

	memset(value, 0, sizeof(*value));
	value->lis.item_type = private->twain_cap->type;
	switch(private->twain_cap->type) {
		case TWTY_INT8:
		case TWTY_UINT8:
			// lis type == LIS_TYPE_INTEGER || STRING
			if (private->twain_cap->possibles != NULL) {
				err = lis_str_to_twain_int(
					private->twain_cap, in_value.string,
					&intvalue
				);
				lis_log_debug(
					"Value: %s -> %d", in_value.string,
					intvalue
				);
			} else {
				lis_log_debug("Value: %d", in_value.integer);
				intvalue = in_value.integer;
			}
			value->lis.value.integer8 = intvalue;
			break;

		case TWTY_INT16:
		case TWTY_UINT16:
			// lis type == LIS_TYPE_INTEGER || STRING
			if (private->twain_cap->possibles != NULL) {
				err = lis_str_to_twain_int(
					private->twain_cap, in_value.string,
					&intvalue
				);
				lis_log_debug(
					"Value: %s -> %d", in_value.string,
					intvalue
				);
			} else {
				lis_log_debug("Value: %d", in_value.integer);
				intvalue = in_value.integer;
			}
			value->lis.value.integer16 = intvalue;
			break;

		case TWTY_INT32:
		case TWTY_UINT32:
			// lis type == LIS_TYPE_INTEGER || STRING
			if (private->twain_cap->possibles != NULL) {
				err = lis_str_to_twain_int(
					private->twain_cap, in_value.string,
					&intvalue
				);
				lis_log_debug(
					"Value: %s -> %d", in_value.string,
					intvalue
				);
			} else {
				lis_log_debug("Value: %d", in_value.integer);
				intvalue = in_value.integer;
			}
			value->lis.value.integer32 = intvalue;
			break;

		case TWTY_BOOL:
			// lis type == LIS_TYPE_BOOL
			lis_log_debug("Value: %d", in_value.boolean);
			value->lis.value.boolean = in_value.boolean;
			break;

		case TWTY_FIX32:
			// lis type == LIS_TYPE_DOUBLE
			lis_log_debug("Value: %f", in_value.dbl);
			value->lis.value.dbl = twain_fix(in_value.dbl);
			lis_log_debug(
				"Value: %d + %d/65536",
				value->lis.value.dbl.Whole,
				value->lis.value.dbl.Frac
			);
			break;

		case TWTY_STR32:
		case TWTY_STR64:
		case TWTY_STR128:
		case TWTY_STR255:
			// lis type == LIS_TYPE_STRING
			strncpy(
				value->lis.value.str, in_value.string,
				sizeof(value->lis.value.str)
			);
			lis_log_debug("Value: %s", value->lis.value.str);
			break;

		default:
			lis_log_error(
				"%s->simple_set_value(%s): Unsupported TWAIN type 0x%X",
				private->item->parent.name, self->name,
				private->twain_cap->type
			);
			err = LIS_ERR_UNSUPPORTED;
			break;
	}

	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s->simple_set_value(%s): value conversion failed:"
			" 0x%X, %s",
			private->item->parent.name, self->name,
			err, lis_strerror(err)
		);
		goto end;
	}

	twrc = DSM_ENTRY(
		&private->item->twain_id,
		DG_CONTROL, DAT_CAPABILITY, MSG_SET,
		&cap
	);

	if (twrc == TWRC_CHECKSTATUS) {
		(*set_flags) |= (
			LIS_SET_FLAG_MUST_RELOAD_OPTIONS
			| LIS_SET_FLAG_MUST_RELOAD_PARAMS
		);
		twrc = TWRC_SUCCESS;
	}

	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"%s->simple_set_value(%s): Failed to get value: 0x%X, %s",
			private->item->parent.name, self->name,
			err, lis_strerror(err)
		);
		goto end;
	}

	lis_log_info(
		"%s->simple_set_value(%s) successful",
		private->item->parent.name, self->name
	);

end:
	// XXX(Jflesch): Based on twain samples, we unlock *after*
	// setting the value
	private->item->impl->entry_points.DSM_MemUnlock(cap.hContainer);

	private->item->impl->entry_points.DSM_MemFree(cap.hContainer);
	return err;
}


static void free_opts(struct lis_twain_item *private)
{
	int i;

	if (private->opts_ptrs != NULL) {
		for (i = 0 ; private->opts_ptrs[i] != NULL ; i++) {
			switch (private->opts_ptrs[i]->constraint.type) {
				case LIS_CONSTRAINT_NONE:
				case LIS_CONSTRAINT_RANGE:
					break;
				case LIS_CONSTRAINT_LIST:
					FREE(private->opts_ptrs[i]->constraint.possible.list.values);
					break;
			}
		}
	}

	FREE(private->opts);
	FREE(private->opts_ptrs);
}


static enum lis_error make_simple_option(
		struct lis_twain_item *item,
		struct lis_twain_option *opt,
		const struct lis_twain_cap *lis_cap,
		TW_CAPABILITY *twain_cap,
		void *container,
		int *nb_opts
	)
{
	opt->twain_cap = lis_cap;
	opt->item = item;

	opt->parent.name = lis_cap->name;
	opt->parent.title = lis_cap->name; // TODO
	opt->parent.desc = lis_cap->name; // TODO
	opt->parent.capabilities = (
		lis_cap->readonly ? 0 : LIS_CAP_SW_SELECT
	);
	opt->parent.value.type = twain_cap_to_lis_type(
		twain_cap, lis_cap, container
	);
	opt->parent.value.unit = LIS_UNIT_NONE; // TODO ?
	opt->parent.fn.get_value = twain_simple_get_value;
	opt->parent.fn.set_value = twain_simple_set_value;

	get_constraint(&opt->parent, lis_cap, twain_cap, container);

	(*nb_opts)++;
	return LIS_OK;
}


static enum lis_error twain_get_options(
		struct lis_item *self,
		struct lis_option_descriptor ***out_descs
	)
{
	struct lis_twain_item *private = LIS_TWAIN_ITEM_PRIVATE(self);
	const struct lis_twain_cap *all_caps;
	int nb_caps, cap_idx;
	int nb_opts = 0, opt_idx;
	void *container;
	TW_UINT16 twrc;
	TW_CAPABILITY cap;

	free_opts(private);

	lis_log_info("%s->get_options() ...", self->name);

	/* There is a Twain capabilities to list all supported capapbilities,
	 * but it's not supported by all data sources. Our safest best here
	 * is to probe available capabilities ourselves by getting their
	 * values.
	 */

	all_caps = lis_twain_get_all_caps(&nb_caps);

	private->opts = calloc(
		nb_caps, sizeof(struct lis_twain_option)
	);
	if (private->opts == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (cap_idx = 0 ; cap_idx < nb_caps ; cap_idx++) {
		memset(&cap, 0, sizeof(cap));
		cap.Cap = all_caps[cap_idx].id;
		cap.ConType = TWON_DONTCARE16;
		lis_log_debug(
			"Probing for capability/option '%s' ...",
			all_caps[cap_idx].name
		);
		twrc = g_dsm_entry_fn(
			&g_app_id, &private->twain_id, DG_CONTROL,
			DAT_CAPABILITY, MSG_GET, &cap
		);
		if (twrc == TWRC_SUCCESS) {
			lis_log_debug(
				"Got option '%s' (container type 0x%X)",
				all_caps[cap_idx].name,
				cap.ConType
			);

			container = private->impl->entry_points.DSM_MemLock(
				cap.hContainer
			);

			make_simple_option(
				private,
				&private->opts[nb_opts], &all_caps[cap_idx],
				&cap, container, &nb_opts
			);

			private->impl->entry_points.DSM_MemUnlock(
				cap.hContainer
			);
			private->impl->entry_points.DSM_MemFree(
				cap.hContainer
			);
		} else {
			lis_log_debug(
				"Option '%s' is not available",
				all_caps[cap_idx].name
			);
		}
	}

	private->opts_ptrs = calloc(
		nb_opts + 1, sizeof(struct lis_option_descriptor *)
	);
	if (private->opts_ptrs == NULL) {
		FREE(private->opts);
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	for(opt_idx = 0 ; opt_idx < nb_opts ; opt_idx++) {
		private->opts_ptrs[opt_idx] = &private->opts[opt_idx].parent;
	}

	lis_log_info("%s->get_options(): %d options", self->name, nb_opts);
	*out_descs = private->opts_ptrs;

	return LIS_OK;
}


static void clean_session(struct lis_twain_session *private)
{
	if (private->item->dsm2app.sem != NULL) {
		CloseHandle(private->item->dsm2app.sem);
		private->item->dsm2app.sem = NULL;
	}
	private->item->session = NULL;
}


static enum lis_error next_page(struct lis_twain_session *private)
{
	TW_UINT16 twrc;
	enum lis_error err;
	int partial_header_len;
	struct lis_scan_parameters params;

	lis_log_info("next_page() ...");

	assert(private->img.mem == NULL);

	memset(&private->img.infos, 0, sizeof(private->img.infos));
	twrc = DSM_ENTRY(
		&(private->item->twain_id), DG_IMAGE, DAT_IMAGEINFO, MSG_GET,
		&private->img.infos
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to get image infos: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		return err;
	}

	twrc = DSM_ENTRY(
		&(private->item->twain_id), DG_IMAGE, DAT_IMAGENATIVEXFER,
		MSG_GET, &(private->img.handle)
	);
	if (twrc != TWRC_SUCCESS && twrc != TWRC_XFERDONE) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to get image: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		return err;
	}

	private->img.mem = private->item->impl->entry_points.DSM_MemLock(
		private->img.handle
	);

	lis_hexdump(private->img.mem, BMP_HEADER_SIZE);

	// Twain DS only return a partial header
	err = twain_get_scan_parameters(&private->parent, &params);
	assert(LIS_IS_OK(err));

	lis_scan_params2bmp(&params, &private->img.header);
	partial_header_len = BMP_HEADER_SIZE - (
		((char *)(&(private->img.header.remaining_header)))
		- ((char *)(&(private->img.header)))
	);
	lis_log_info("partial header length: %d", partial_header_len);
	memcpy(
		&(private->img.header.remaining_header),
		private->img.mem,
		partial_header_len
	);
	lis_hexdump(&private->img.header, BMP_HEADER_SIZE);

	// From TWAIN example
	// If the driver did not fill in the biSizeImage field, then compute it
	// Each scan line of the image is aligned on a DWORD (32bit) boundary
	if(private->img.header.pixel_data_size == 0) {
		private->img.header.pixel_data_size = (
			(((private->img.header.width
				* private->img.header.nb_bits_per_pixel)
				+ 31) & ~31) / 8)
				* private->img.header.height;
	}

	private->img.mem = ((char *)private->img.mem) + partial_header_len;

	private->img.header_size = BMP_HEADER_SIZE;
	private->img.header_read = 0;
	// TODO(Jflesch): endianess of BMP header
	private->img.img_size = private->img.header.pixel_data_size;
	lis_log_info(
		"Expecting %d bytes of data", private->img.img_size
	);
	private->img.img_read = 0;

	lis_log_info("next_page(): OK");
	return LIS_OK;
}


static void end_page(struct lis_twain_session *private)
{
	TW_UINT16 twrc;
	enum lis_error err;
	TW_PENDINGXFERS xfers;
	TW_USERINTERFACE ui;

	lis_log_info("end_page() ...");

	if (private->img.mem != NULL)  {
		private->item->impl->entry_points.DSM_MemUnlock(
			private->img.handle
		);
		private->item->impl->entry_points.DSM_MemFree(
			private->img.handle
		);
		private->img.mem = NULL;
	}

	if (private->cancelled) {
		goto check_end;
	}

	memset(&xfers, 0, sizeof(xfers));
	twrc = DSM_ENTRY(
		&(private->item->twain_id), DG_CONTROL, DAT_PENDINGXFERS,
		MSG_ENDXFER, &xfers
	);
	if (twrc != TWRC_SUCCESS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"Failed to look for next pages: 0x%X -> 0x%X, %s",
			twrc, err, lis_strerror(err)
		);
		private->xfer_pending = FALSE;
		return;
	}

	private->xfer_pending = (xfers.Count > 0);
	lis_log_info(
		"end_page(): OK (next ? %d ; cancelled ? %d)",
		(int)private->xfer_pending,
		(int)private->cancelled
	);

check_end:
	memset(&ui, 0, sizeof(ui));
	ui.ShowUI = FALSE;
	ui.ModalUI = TRUE;
	ui.hParent = GetDesktopWindow();

	if (!private->xfer_pending || !private->cancelled) {
		lis_log_info("End of scan --> disabling DS");
		twrc = DSM_ENTRY(
			&(private->item->twain_id), DG_CONTROL,
			DAT_USERINTERFACE, MSG_DISABLEDS, &ui
		);
		if (twrc != TWRC_SUCCESS) {
			lis_log_warning("Failed to disable datasource");
		}
	}
}


static enum lis_error twain_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *parameters
	)
{
	struct lis_twain_session *private = LIS_TWAIN_SESSION_PRIVATE(self);
	lis_log_info("get_scan_parameters() ...");
	memset(parameters, 0, sizeof(*parameters));
	parameters->format = LIS_IMG_FORMAT_BMP;
	parameters->width = private->img.infos.ImageWidth;
	parameters->height = private->img.infos.ImageLength;
	parameters->image_size = (
		private->img.infos.ImageWidth
		* private->img.infos.ImageLength
		* 3
	);

	lis_log_info("get_scan_parameters(): OK");
	return LIS_OK;
}


static int twain_end_of_feed(struct lis_scan_session *self)
{
	struct lis_twain_session *private = LIS_TWAIN_SESSION_PRIVATE(self);
	int r;
	r = (!private->xfer_pending && !private->cancelled);
	lis_log_info("end_of_feed() ? %d", r);
	return r;
}


static int twain_end_of_page(struct lis_scan_session *self)
{
	struct lis_twain_session *private = LIS_TWAIN_SESSION_PRIVATE(self);
	int r;
	r = (private->img.mem == NULL && !private->cancelled);
	lis_log_info("end_of_page() ? %d", r);
	return r;
}


static enum lis_error twain_scan_read(
		struct lis_scan_session *self, void *out_buffer,
		size_t *buffer_size
	)
{
	enum lis_error err;
	struct lis_twain_session *private = LIS_TWAIN_SESSION_PRIVATE(self);
	size_t to_copy;

	lis_log_info("scan_read(%d B) ...", (int)(*buffer_size));
	if (private->img.mem == NULL) {
		err = next_page(private);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"scan_read(): next_page() failed: 0x%X, %s",
				err, lis_strerror(err)
			);
			return err;
		}
	}

	if (private->cancelled) {
		end_page(private);
		return LIS_ERR_CANCELLED;
	}

	if (private->img.header_read < private->img.header_size) {
		to_copy = MIN(
			((int)(*buffer_size)),
			private->img.header_size - private->img.header_read
		);
		lis_log_debug(
			"scan_read(header: %p, %d/%d, %d B) ...",
			(void *)&private->img.header,
			private->img.header_read,
			private->img.header_size,
			(int)(to_copy)
		);
		memcpy(
			out_buffer,
			((char *)(&private->img.header)) + private->img.header_read,
			to_copy
		);
		private->img.header_read += to_copy;
		(*buffer_size) = to_copy;

	} else if (private->img.img_read < private->img.img_size) {

		to_copy = MIN(
			((int)(*buffer_size)),
			private->img.img_size - private->img.img_read
		);
		lis_log_debug(
			"scan_read(content: %p, %d/%d, %d B) ...",
			private->img.mem,
			private->img.img_read,
			private->img.img_size,
			(int)(to_copy)
		);
		memcpy(
			out_buffer,
			((char *)(private->img.mem)) + private->img.img_read,
			to_copy
		);
		private->img.img_read += to_copy;
		(*buffer_size) = to_copy;

	}

	if (private->img.img_read >= private->img.img_size) {
		end_page(private);
	}
	lis_log_info("scan_read(): OK (%d B)", (int)(*buffer_size));
	return LIS_OK;
}


static void twain_cancel(struct lis_scan_session *self)
{
	struct lis_twain_session *private = LIS_TWAIN_SESSION_PRIVATE(self);
	private->cancelled = TRUE;
}


static enum lis_error wait_ready(struct lis_twain_item *private)
{
	MSG win_msg;
	TW_UINT16 twrc, msg;
	TW_EVENT event;

	if (private->use_callback) {
		lis_log_info("Twain scan: Waiting for DSM callback ...");
		while(TRUE) {
			WaitForSingleObject(
				private->dsm2app.sem, INFINITE /* timeout */
			);

			msg = private->dsm2app.msgs[private->dsm2app.read_idx];
			private->dsm2app.read_idx++;
			private->dsm2app.read_idx %= MAX_MSG;
			lis_log_debug("Message: 0x%X", (int)msg);

			switch(msg) {
				case MSG_XFERREADY:
					return LIS_OK;
				case MSG_CLOSEDSREQ:
				case MSG_CLOSEDSOK:
				case MSG_NULL:
					break;
				default:
					lis_log_error(
						"Unexpected event: 0x%X",
						(int)msg
					);
					return LIS_ERR_IO_ERROR;
			}
		}
		return LIS_OK;
	} else {
		lis_log_debug("GetMessage() ...");
		while (GetMessage(&win_msg, NULL /* handle */,
				0 /* min msg */, 0 /* max msg */)) {
			lis_log_debug("GetMessage() !");
			memset(&event, 0, sizeof(event));
			event.pEvent = &win_msg;
			event.TWMessage = MSG_NULL;
			twrc = DSM_ENTRY(
				&private->twain_id, DG_CONTROL, DAT_EVENT,
				MSG_PROCESSEVENT, &event
			);

			lis_log_debug("Twrc: 0x%X", twrc);
			if (twrc == TWRC_DSEVENT) {
				lis_log_debug(
					"Message: 0x%X", (int)event.TWMessage
				);
				switch(event.TWMessage) {
					case MSG_XFERREADY:
						return LIS_OK;
					case MSG_CLOSEDSREQ:
					case MSG_CLOSEDSOK:
					case MSG_NULL:
						break;
					default:
						lis_log_error(
							"Unexpected event: 0x%X",
							(int)event.TWMessage
						);
						return LIS_ERR_IO_ERROR;
				}
			} else {
				TranslateMessage(&win_msg);
				DispatchMessage(&win_msg);
			}
		}
		lis_log_warning("Got message WM_QUIT");
		return LIS_ERR_CANCELLED;
	}
}


static enum lis_error twain_scan_start(
		struct lis_item *self, struct lis_scan_session **out_session
	)
{
	struct lis_twain_item *private = LIS_TWAIN_ITEM_PRIVATE(self);
	struct lis_twain_session *session;
	TW_USERINTERFACE ui;
	TW_UINT16 twrc;
	enum lis_error err;

	session = calloc(1, sizeof(struct lis_twain_session));
	if (session == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(
		&session->parent, &g_twain_session_template,
		sizeof(session->parent)
	);
	session->item = private;

	memset(&private->dsm2app, 0, sizeof(private->dsm2app));

	private->dsm2app.sem = CreateSemaphore(
		NULL, // default security attributes
		0, // initial count
		MAX_MSG - 1, // max sem count
		"twain_dsm2app"
	);
	if (private->dsm2app.sem == NULL) {
		lis_log_error("Failed to create semaphore");
		return LIS_ERR_NO_MEM;
	}

	memset(&ui, 0, sizeof(ui));
	ui.ShowUI = FALSE;
	ui.ModalUI = TRUE;
	ui.hParent = GetDesktopWindow();

	twrc = DSM_ENTRY(
		&private->twain_id, DG_CONTROL, DAT_USERINTERFACE,
		MSG_ENABLEDS, &ui
	);
	if (twrc != TWRC_SUCCESS && twrc != TWRC_CHECKSTATUS) {
		err = twrc_to_lis_error(twrc);
		lis_log_error(
			"%s->scan_start(): MSG_ENABLEDS failed:"
			" %d -> 0x%X, %s",
			self->name, twrc, err, lis_strerror(err)
		);
		clean_session(session);
		return err;
	}

	lis_log_info("Waiting for scan transfer");
	err = wait_ready(private);
	if (LIS_IS_ERROR(err)) {
		clean_session(session);
		return err;
	}

	lis_log_info("Scan transfer ready");

	session->xfer_pending = TRUE;
	*out_session = &session->parent;
	return next_page(session);
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
