#include <assert.h>
#include <stdlib.h>

#define DEFINE_WIA_PROPID_TO_NAME

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

#include "properties.h"
#include "transfer.h"
#include "util.h"
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


struct wiall_opt_private {
	struct lis_option_descriptor parent;

	struct wiall_item_private *item;

	const struct lis_wia2lis_property *wia2lis;

	char *last_value;
};
#define WIALL_OPT_PRIVATE(impl) ((struct wiall_opt_private *)(impl))


struct wiall_item_private {
	struct lis_item parent;
	LisIWiaItem2 *wia_item;
	IWiaPropertyStorage *wia_props;

	struct wiall_item_private *children;
	struct lis_item **children_ptrs;

	struct wiall_opt_private *opts;
	struct lis_option_descriptor **opts_ptrs;
	int nb_opts;

	struct wia_transfer *scan;
};
#define WIALL_ITEM_PRIVATE(impl) ((struct wiall_item_private *)(impl))


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


static enum lis_error wiall_item_get_children(
		struct lis_item *self, struct lis_item ***children
	);
static enum lis_error wiall_item_root_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	);
static enum lis_error wiall_item_child_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	);
static enum lis_error wiall_item_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	);
static void wiall_item_root_close(struct lis_item *self);
static void wiall_item_child_close(struct lis_item *self);


static struct lis_item g_item_root_template = {
	.get_children = wiall_item_get_children,
	.get_options = wiall_item_root_get_options,
	.scan_start = wiall_item_scan_start,
	.close = wiall_item_root_close,
};
static struct lis_item g_item_child_template = {
	.get_children = wiall_item_get_children,
	.get_options = wiall_item_child_get_options,
	.scan_start = wiall_item_scan_start,
	.close = wiall_item_child_close,
};


static enum lis_error wiall_opt_get_value(
		struct lis_option_descriptor *self, union lis_value *value
	);
static enum lis_error wiall_opt_set_value(
		struct lis_option_descriptor *self, union lis_value value,
		int *set_flags
	);


static struct lis_option_descriptor g_opt_template = {
	.fn = {
		.get_value = wiall_opt_get_value,
		.set_value = wiall_opt_set_value,
	},
};


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


static char *custom_strchr(char *input, char *separators)
{
	char *s;

	for (; (*input) != '\0' ; input++) {
		for (s = separators ; (*s) != '\0' ; s++) {
			if ((*input) == (*s)) {
				return input;
			}
		}
	}

	return NULL;
}


#define compare_guid(a, b) (memcmp((a), (b), sizeof(*(a))) == 0)


static enum lis_error get_device_descriptor(
		IWiaPropertyStorage *in_props,
		struct lis_device_descriptor *out_props
	)
{
	char *dev_name;
	char *model;
	enum lis_error err;
	HRESULT hr;
	unsigned int i;
	static const PROPSPEC input[] = {
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
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_DPA_CONNECT_STATUS,
		},
	};
	PROPVARIANT output[LIS_COUNT_OF(input)];

	hr = in_props->lpVtbl->ReadMultiple(in_props, LIS_COUNT_OF(output), input, output);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"ReadMultiple failed: 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		return err;
	}

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
			err = LIS_OK;
			goto err;
	}

	out_props->dev_id = lis_propvariant2char(&output[0]);

	// ASSUMPTION(Jflesch): only the property 'dev_name' seems to be
	// reliable to get the manufacturer name and model name.
	// See: https://openpaper.work/fr/scanner_db/report/337/
	// --> we assume the first word in dev_name is the manufacturer
	// and the rest is the model name, unless there is only one word
	// in dev_name.
	dev_name = lis_propvariant2char(&output[2]);
	model = custom_strchr(dev_name, " 0123456789-_");
	if (model != NULL) {
		model[0] = '\0';
		out_props->model = strdup(model + 1);
		out_props->vendor = dev_name;
	} else {
		out_props->vendor = lis_propvariant2char(&output[1]);
		out_props->model = dev_name;
	}

	err = LIS_OK;

err:
	for (i = 0 ; i < LIS_COUNT_OF(output) ; i++) {
		PropVariantClear(&output[i]);
	}
	return err;
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
			props->lpVtbl->Release(props);
			return err;
		}
		props->lpVtbl->Release(props);
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
	lis_log_info("wiall_list_devices() successful");
	return LIS_OK;
}


static void free_opts(struct wiall_item_private *private)
{
	int i, j;
	if (private->opts_ptrs != NULL) {
		for (i = 0 ; private->opts_ptrs[i] != NULL ; i++) {
			if (private->opts_ptrs[i]->constraint.type == LIS_CONSTRAINT_LIST) {
				if (private->opts_ptrs[i]->value.type == LIS_TYPE_STRING) {
					for (j = 0 ; j < private->opts_ptrs[i]->constraint.possible.list.nb_values ; j++) {
						FREE(private->opts_ptrs[i]->constraint.possible.list.values[j].string);
					}
				}
				FREE(private->opts_ptrs[i]->constraint.possible.list.values);
				private->opts_ptrs[i]->constraint.possible.list.nb_values = 0;
			}
			FREE(private->opts_ptrs[i]->desc);
		}

		FREE(private->opts_ptrs);
	}
	FREE(private->opts);
}


static void free_children(struct wiall_item_private *private)
{
	int i;

	if (private->children_ptrs != NULL) {
		for (i = 0 ; private->children_ptrs[i] != NULL ; i++) {
			free_opts(&private->children[i]);
			private->children[i].wia_props->lpVtbl->Release(
				private->children[i].wia_props
			);
			private->children[i].wia_props = NULL;
			private->children[i].wia_item->lpVtbl->Release(
				private->children[i].wia_item
			);
			private->children[i].wia_item = NULL;
			FREE(private->children[i].parent.name);
			free_children(&private->children[i]);
		}
		FREE(private->children);
		FREE(private->children_ptrs);
	}
}


static enum lis_error fill_in_item_infos(struct wiall_item_private *private)
{
	HRESULT hr;
	enum lis_error err;
	unsigned int i;
	static const PROPSPEC input[] = {
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPA_FULL_ITEM_NAME,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = LIS_WIA_IPA_ITEM_CATEGORY,
		},
	};
	PROPVARIANT output[LIS_COUNT_OF(input)] = {0};

	memcpy(
		&private->parent,
		&g_item_child_template,
		sizeof(private->parent)
	);

	lis_log_debug("child_item->QueryInterface(IWiaPropertyStorage) ...");
	hr = private->wia_item->lpVtbl->QueryInterface(
		private->wia_item,
		&IID_IWiaPropertyStorage,
		(void **)&private->wia_props
	);
	lis_log_debug(
		"child_item->QueryInterface(IWiaPropertyStorageP): 0x%lX", hr
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"child_item->QueryInterface(WiaPropertyStorage)"
			" failed: 0X%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto err;
	}

	hr = private->wia_props->lpVtbl->ReadMultiple(
		private->wia_props,
		LIS_COUNT_OF(output),
		input, output
	);
	if (FAILED(hr)) {
		private->wia_props->lpVtbl->Release(private->wia_props);
		private->wia_props = NULL;
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"child_properties->ReadMultiple()"
			" failed: 0X%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto err;
	}

	assert(output[0].vt == VT_BSTR);
	assert(output[1].vt == VT_CLSID);

	private->parent.name = lis_bstr2cstr(output[0].bstrVal);

	if (compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FINISHED_FILE)
			|| compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FOLDER)) {
		lis_log_warning(
			"Unsupported source type for child '%s'",
			private->parent.name
		);
		private->wia_props->lpVtbl->Release(private->wia_props);
		private->wia_props = NULL;
		err = LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
		goto err;
	} else if (compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FLATBED)) {
		lis_log_info("Child '%s' is a flatbed", private->parent.name);
		private->parent.type = LIS_ITEM_FLATBED;
	} else if (compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FEEDER)
			|| compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FEEDER_FRONT)
			|| compare_guid(output[1].puuid, &LIS_WIA_CATEGORY_FEEDER_BACK)) {
		lis_log_info("Child '%s' is an ADF", private->parent.name);
		private->parent.type = LIS_ITEM_ADF;
	} else {
		// XXX(Jflesch): On the Canon Pixma MG6850, this the item
		// '0000\\Root\\Auto'. It doesn't provide any of the required options
		// for Libinsane (xres, yres, etc). --> We want to drop it.
		// https://openpaper.work/scannerdb/report/313/
		// This is unfortunately the best way I've found to drop it.
		// I hope it won't drop any other source that could be used with
		// Libinsane :/
		lis_log_warning(
			"Unknown source type for child '%s'. Ignoring it",
			private->parent.name
		);
		private->wia_props->lpVtbl->Release(private->wia_props);
		private->wia_props = NULL;
		err = LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
		goto err;
	}

	err = LIS_OK;

err:
	for (i = 0 ; i < LIS_COUNT_OF(output) ; i++) {
		PropVariantClear(&output[i]);
	}
	return err;
}


static enum lis_error wiall_item_get_children(
		struct lis_item *self, struct lis_item ***out_children
	)
{
	struct wiall_item_private *private = WIALL_ITEM_PRIVATE(self);
	HRESULT hr;
	enum lis_error err;
	LisIEnumWiaItem2 *item_enum = NULL;
	unsigned long nb_children;
	unsigned long tmp;
	unsigned int i;

	lis_log_debug(
		"%s->get_children() ...",
		private->parent.name
	);

	free_children(private);

	lis_log_debug("%s->EnumChildItems() ...", self->name);
	hr = private->wia_item->lpVtbl->EnumChildItems(
		private->wia_item,
		NULL, // GUID of child category ; NULL == all
		&item_enum
	);
	lis_log_debug("%s->EnumChildItems(): 0x%lX", self->name, hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"%s->EnumChjldItems() failed: 0x%lX -> 0x%X, %s",
			self->name, hr, err, lis_strerror(err)
		);
		lis_log_debug("wiall_item_get_children() failed");
		return err;
	}

	hr = item_enum->lpVtbl->GetCount(item_enum, &nb_children);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"%s->EnumChjldItems()->GetCount() failed:"
			" 0x%lX -> 0x%X, %s",
			self->name, hr, err, lis_strerror(err)
		);
		lis_log_debug("wiall_item_get_children() failed");
		return err;
	}

	lis_log_info("Found %lu sources", nb_children);

	private->children_ptrs = calloc(
		nb_children + 1, sizeof(struct lis_item *)
	);
	if (nb_children == 0) {
		item_enum->lpVtbl->Release(item_enum);
		*out_children = private->children_ptrs;
		return LIS_OK;
	}
	private->children = calloc(
		nb_children, sizeof(struct wiall_item_private)
	);
	if (private->children_ptrs == NULL || private->children == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (i = 0 ; i < nb_children ; i++) {
		tmp = 1;
		hr = item_enum->lpVtbl->Next(
			item_enum,
			1, // celt ; number of elements
			&(private->children[i].wia_item),
			&tmp
		);
		if (hr == S_FALSE || tmp == 0) {
			lis_log_warning(
				"Got less children than expected (%u < %lu)",
				i, nb_children
			);
			break;
		}
		if (FAILED(hr)) {
			err = hresult_to_lis_error(hr);
			lis_log_error(
				"%s->EnumChjldItems()->Next() failed:"
				" 0x%lX -> 0x%X, %s",
				self->name, hr, err, lis_strerror(err)
			);
			lis_log_debug("wiall_item_get_children() failed");
			item_enum->lpVtbl->Release(item_enum);
			return err;
		}

		err = fill_in_item_infos(&private->children[i]);
		if (LIS_IS_ERROR(err)) {
			// skipping this one
			private->children[i].wia_item->lpVtbl->Release(
				private->children[i].wia_item
			);
			i--;
			nb_children--;
			continue;
		}
		private->children_ptrs[i] = &private->children[i].parent;
	}

	item_enum->lpVtbl->Release(item_enum);
	*out_children = private->children_ptrs;
	lis_log_info(
		"%s->get_children() successful",
		private->parent.name
	);
	return LIS_OK;
}


static char *get_opt_desc(PROPID propid)
{
	unsigned int i;

	for (i = 0 ; i < LIS_COUNT_OF(g_wiaPropIdToName) ; i++) {
		if (g_wiaPropIdToName[i].propid == propid) {
			return lis_bstr2cstr(g_wiaPropIdToName[i].pszName);
		}
	}

	return strdup("no description");
}


static enum lis_error load_opts(bool root, struct wiall_item_private *private)
{
	IEnumSTATPROPSTG *prop_enum = NULL;
	STATPROPSTG wia_prop;
	HRESULT hr;
	enum lis_error err;
	unsigned long nb_opts = 0;
	int opt_idx;
	unsigned long nb_results;

	lis_log_debug(
		"%s->get_options() ...",
		private->parent.name
	);

	free_opts(private);

	lis_log_debug("IWiaPropertyStorage->GetCount() ...");
	hr = private->wia_props->lpVtbl->GetCount(private->wia_props, &nb_opts);
	lis_log_debug("IWiaPropertyStorage->GetCount(): 0x%lX (%lu)", hr, nb_opts);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"IWiaPropertyStorage->GetCount() failed: "
			"0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto err;
	}
	lis_log_debug(
		"%s->get_options(): got %lu options",
		private->parent.name, nb_opts
	);

	private->opts_ptrs = calloc(
		nb_opts + 1, sizeof(struct lis_option_descriptor *)
	);
	if (nb_opts <= 0) {
		return LIS_OK;
	}

	private->opts = calloc(nb_opts, sizeof(struct wiall_opt_private));
	if (private->opts == NULL || private->opts_ptrs == NULL) {
		lis_log_error("Out of memory");
		err = LIS_ERR_NO_MEM;
		goto err;
	}

	lis_log_debug("IWiaPropertyStorage->Enum() ...");
	hr = private->wia_props->lpVtbl->Enum(private->wia_props, &prop_enum);
	lis_log_debug("IWiaPropertyStorage->Enum(): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"IWiaPropertyStorage->Enum() failed: "
			"0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto err;
	}

	for (opt_idx = 0 ;
				(hr = prop_enum->lpVtbl->Next(
					prop_enum, 1 /* celt */, &wia_prop,
					&nb_results
				)) != S_FALSE ;
				opt_idx++
			) {
		if (FAILED(hr)) {
			err = hresult_to_lis_error(hr);
			lis_log_error(
				"IEnumSTATPROPSTG->Next() failed: "
				"0x%lX -> 0x%X, %s",
				hr, err, lis_strerror(err)
			);
			goto err;
		}
		// A name for the option may or may not be provided
		// --> this is useless
		CoTaskMemFree(wia_prop.lpwstrName);

		private->opts[opt_idx].wia2lis = lis_wia2lis_get_property(
			root, wia_prop.propid
		);
		if (private->opts[opt_idx].wia2lis == NULL) {
			// unknown property
			opt_idx--;
			nb_opts--;
			continue;
		}
		if (wia_prop.vt != private->opts[opt_idx].wia2lis->wia.type) {
			lis_log_warning(
				"Unexpected type for prop '%s' (%lu)"
				" (type %d instead of %d)",
				private->opts[opt_idx].wia2lis->lis.name,
				wia_prop.propid, wia_prop.vt,
				private->opts[opt_idx].wia2lis->wia.type
			);
			opt_idx--;
			nb_opts--;
			continue;
		}

		memcpy(
			&private->opts[opt_idx].parent, &g_opt_template,
			sizeof(private->opts[opt_idx].parent)
		);

		private->opts[opt_idx].parent.name = \
			private->opts[opt_idx].wia2lis->lis.name;
		private->opts[opt_idx].parent.title = \
			private->opts[opt_idx].wia2lis->lis.name;
		private->opts[opt_idx].parent.desc =
			get_opt_desc(private->opts[opt_idx].wia2lis->wia.id);
		private->opts[opt_idx].parent.value.type = \
			private->opts[opt_idx].wia2lis->lis.type;
		private->opts[opt_idx].item = private;

		private->opts_ptrs[opt_idx] = &private->opts[opt_idx].parent;
	}

	private->nb_opts = opt_idx;
	lis_log_info(
		"%s->get_options(): got %d/%ld options",
		private->parent.name, opt_idx, nb_opts
	);
	prop_enum->lpVtbl->Release(prop_enum);
	return LIS_OK;

err:
	FREE(private->opts);
	FREE(private->opts_ptrs);
	if (prop_enum != NULL) {
		prop_enum->lpVtbl->Release(prop_enum);
	}
	return err;
}


static enum lis_error load_opt_capabilities(
		struct wiall_item_private *private,
		const unsigned long *propflags
	)
{
	int i;

	for (i = 0 ; i < private->nb_opts ; i++) {
		private->opts[i].parent.capabilities = 0;
		if (!(propflags[i] & WIA_PROP_READ)) {
			private->opts[i].parent.capabilities |= LIS_CAP_INACTIVE;
		}
		if (propflags[i] & WIA_PROP_WRITE) {
			private->opts[i].parent.capabilities |= LIS_CAP_SW_SELECT;
		}
	}

	return LIS_OK;
}


static enum lis_error load_opt_constraints(
		struct wiall_item_private *private,
		const unsigned long *propflags,
		const PROPVARIANT *propvariants
	)
{
	int i;
	enum lis_error err;

	for (i = 0 ; i < private->nb_opts ; i++) {
		private->opts[i].parent.constraint.type = LIS_CONSTRAINT_NONE;
		if (private->opts[i].wia2lis->possibles != NULL) {
			private->opts[i].parent.constraint.type = LIS_CONSTRAINT_LIST;
			err = lis_wia2lis_get_possibles(
				private->opts[i].wia2lis,
				propvariants[i],
				&private->opts[i].parent.constraint.possible.list
			);
		} else if (propflags[i] & WIA_PROP_RANGE) {
			private->opts[i].parent.constraint.type = LIS_CONSTRAINT_RANGE;
			err = lis_wia2lis_get_range(
				private->opts[i].wia2lis,
				propvariants[i],
				&private->opts[i].parent.constraint.possible.range
			);
		} else if (propflags[i] & WIA_PROP_LIST) {
			private->opts[i].parent.constraint.type = LIS_CONSTRAINT_LIST;
			err = lis_wia2lis_get_list(
				private->opts[i].wia2lis,
				propvariants[i],
				&private->opts[i].parent.constraint.possible.list
			);
		}
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to parse constraint for option '%s':"
				" 0x%X, %s",
				private->opts[i].parent.name,
				err, lis_strerror(err)
			);
			return err;
		}
	}

	return LIS_OK;
}


static enum lis_error load_opt_attributes(struct wiall_item_private *private)
{
	HRESULT hr;
	enum lis_error err;
	PROPSPEC *propspecs = NULL;
	PROPVARIANT *propvariants = NULL;
	unsigned long *propflags = NULL;
	int i;

	propspecs = GlobalAlloc(GPTR, sizeof(PROPSPEC) * private->nb_opts);
	propvariants = GlobalAlloc(GPTR, sizeof(PROPVARIANT) * private->nb_opts);
	propflags = GlobalAlloc(GPTR, sizeof(unsigned long) * private->nb_opts);
	if (propspecs == NULL || propvariants == NULL || propflags == NULL) {
		lis_log_error("Out of memory");
		err = LIS_ERR_NO_MEM;
		goto err;
	}

	for (i = 0 ; i < private->nb_opts ; i++) {
		propspecs[i].ulKind = PRSPEC_PROPID;
		propspecs[i].propid = private->opts[i].wia2lis->wia.id;
	}

	lis_log_debug("IWiaPropertyStorage->GetPropertyAttribute(all properties) ...");
	hr = private->wia_props->lpVtbl->GetPropertyAttributes(
		private->wia_props, private->nb_opts, propspecs, propflags, propvariants
	);
	lis_log_debug("IWiaPropertyStorage->GetPropertyAttribute(all properties): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"IWiaPropertyStorage->GetPropertyAttribute(all properties) failed:"
			" 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto err;
	}

	err = load_opt_capabilities(private, propflags);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	err = load_opt_constraints(private, propflags, propvariants);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	err = LIS_OK;

err:
	if (propflags != NULL) {
		GlobalFree(propflags);
	}
	if (propspecs != NULL) {
		GlobalFree(propspecs);
	}
	if (propvariants != NULL) {
		for (i = 0 ; i < private->nb_opts ; i++) {
			PropVariantClear(&propvariants[i]);
		}
		GlobalFree(propvariants);
	}
	return err;
}


static enum lis_error get_options(
		bool root,
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	struct wiall_item_private *private = WIALL_ITEM_PRIVATE(self);
	enum lis_error err;

	lis_log_debug(
		"%s(%s)->get_options() ...",
		self->name, root ? "root" : "child"
	);

	err = load_opts(root, private);
	if (LIS_IS_ERROR(err)) {
		lis_log_debug(
			"%s->get_options() failed: 0x%X, %s",
			self->name, err, lis_strerror(err)
		);
		return err;
	}

	err = load_opt_attributes(private);
	if (LIS_IS_ERROR(err)) {
		lis_log_debug(
			"%s->get_options() failed: 0x%X, %s",
			self->name, err, lis_strerror(err)
		);
		return err;
	};

	lis_log_debug("%s->get_options() done", self->name);
	*descs = private->opts_ptrs;
	return LIS_OK;
}


static enum lis_error wiall_item_root_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	return get_options(TRUE /* root */, self, descs);
}


static enum lis_error wiall_item_child_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	return get_options(FALSE /* !root */, self, descs);
}


static void wiall_item_root_close(struct lis_item *self)
{
	struct wiall_item_private *private = WIALL_ITEM_PRIVATE(self);

	free_opts(private);
	free_children(private);
	FREE(private->parent.name);
	private->wia_props->lpVtbl->Release(private->wia_props);
	private->wia_item->lpVtbl->Release(private->wia_item);
	FREE(private);
}


static void wiall_item_child_close(struct lis_item *self)
{
	// no-op
	LIS_UNUSED(self);
}


static enum lis_error wiall_get_device(
		struct lis_api *self, const char *in_dev_id,
		struct lis_item **out_item
	)
{
	struct wiall_impl_private *private = WIALL_IMPL_PRIVATE(self);
	struct wiall_item_private *item;
	HRESULT hr;
	enum lis_error err;
	BSTR dev_id;

	lis_log_debug("wiall_get_device(%s) ...", in_dev_id);

	if (private->wia_dev_mgr == NULL) {
		err = wiall_init(private);
		if (LIS_IS_ERROR(err)) {
			lis_log_debug("wiall_get_device(%s) failed", in_dev_id);
			return err;
		}
	}

	dev_id = lis_cstr2bstr(in_dev_id);
	if (dev_id == NULL) {
		lis_log_error("Failed to convert device id: %s", in_dev_id);
		lis_log_debug("wiall_get_device(%s) failed", in_dev_id);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	item = calloc(1, sizeof(struct wiall_item_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	lis_log_debug("WiaItem2->CreateDevice(%s) ...", in_dev_id);
	hr = private->wia_dev_mgr->lpVtbl->CreateDevice(
		private->wia_dev_mgr,
		0, // reserved
		dev_id,
		&item->wia_item
	);
	SysFreeString(dev_id);
	lis_log_debug(
		"WiaItem2->CreateDevice(%s): 0x%lX",
		in_dev_id, hr
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_debug(
			"WiaItem2->CreateDevice(%s) failed: 0x%lX -> 0x%X, %s",
			in_dev_id, hr, err, lis_strerror(err)
		);
		lis_log_debug("wiall_get_device(%s) failed", in_dev_id);
		FREE(item);
		return err;
	}

	memcpy(&item->parent, &g_item_root_template, sizeof(item->parent));
	item->parent.type = LIS_ITEM_DEVICE;

	lis_log_debug("root_item->QueryInterface(IWiaPropertyStorage) ...");
	hr = item->wia_item->lpVtbl->QueryInterface(
		item->wia_item,
		&IID_IWiaPropertyStorage,
		(void **)&item->wia_props
	);
	lis_log_debug(
		"root_item->QueryInterface(IWiaPropertyStorageP): 0x%lX", hr
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"root_item->QueryInterface(WiaPropertyStorage)"
			" failed: 0X%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		item->wia_item->lpVtbl->Release(item->wia_item);
		FREE(item);
		return err;
	}

	item->parent.name = strdup(in_dev_id);
	*out_item = &item->parent;
	lis_log_info("wiall_get_device(%s) successful", in_dev_id);
	return LIS_OK;
}


static enum lis_error wiall_opt_get_value(
		struct lis_option_descriptor *self, union lis_value *out_value
	)
{
	struct wiall_opt_private *private = WIALL_OPT_PRIVATE(self);
	HRESULT hr;
	enum lis_error err;
	PROPSPEC propspec = {
		.ulKind = PRSPEC_PROPID,
		.propid = private->wia2lis->wia.id,
	};
	PROPVARIANT propvariant;

	FREE(private->last_value);

	lis_log_debug(
		"%s->%s->get_value() ...",
		private->item->parent.name,
		private->wia2lis->lis.name
	);
	lis_log_debug(
		"%s->ReadMultiple(%lu (%s)) ...",
		private->item->parent.name, private->wia2lis->wia.id,
		private->wia2lis->lis.name
	);
	hr = private->item->wia_props->lpVtbl->ReadMultiple(
		private->item->wia_props,
		1 /* cpspec */, &propspec, &propvariant
	);
	lis_log_debug(
		"%s->ReadMultiple(%lu (%s)): 0x%lX",
		private->item->parent.name, private->wia2lis->wia.id,
		private->wia2lis->lis.name, hr
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_warning(
			"Failed to read property %lu, %s from item %s:"
			" 0x%lX -> 0x%X (%s)",
			private->wia2lis->wia.id, private->wia2lis->lis.name,
			private->item->parent.name,
			hr, err, lis_strerror(err)
		);
		return err;
	}

	err = lis_convert_wia2lis(
		private->wia2lis, &propvariant, out_value, &private->last_value
	);
	PropVariantClear(&propvariant);
	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
			"Failed to convert property value %lu, %s from item %s:"
			" 0x%X (%s)",
			private->wia2lis->wia.id, private->wia2lis->lis.name,
			private->item->parent.name,
			err, lis_strerror(err)
		);
		return err;
	}

	lis_log_info(
		"%s->%s->get_value() successful",
		private->item->parent.name,
		private->wia2lis->lis.name
	);
	return LIS_OK;
}


static enum lis_error wiall_opt_set_value(
		struct lis_option_descriptor *self, union lis_value value,
		int *set_flags
	)
{
	struct wiall_opt_private *private = WIALL_OPT_PRIVATE(self);
	HRESULT hr;
	enum lis_error err;
	PROPSPEC propspec = {
		.ulKind = PRSPEC_PROPID,
		.propid = private->wia2lis->wia.id,
	};
	PROPVARIANT propvariant;

	/* JFlesch> We don't get any feedback from WriteMultiple()
	 * --> we assume the worst case possible here
	 */
	*set_flags = (
		LIS_SET_FLAG_INEXACT
		| LIS_SET_FLAG_MUST_RELOAD_OPTIONS
		| LIS_SET_FLAG_MUST_RELOAD_PARAMS
	);

	lis_log_debug(
		"%s->%s->set_value() ...",
		private->item->parent.name,
		private->wia2lis->lis.name
	);

	err = lis_convert_lis2wia(
		private->wia2lis,
		value,
		&propvariant
	);
	if (LIS_IS_ERROR(err)) {
		lis_log_info(
			"%s->%s->set_value() failed: 0x%X, %s",
			private->item->parent.name,
			private->wia2lis->lis.name,
			err, lis_strerror(err)
		);
		return err;
	}

	lis_log_debug(
		"%s->WriteMultiple(%lu (%s)) ...",
		private->item->parent.name, private->wia2lis->wia.id,
		private->wia2lis->lis.name
	);
	hr = private->item->wia_props->lpVtbl->WriteMultiple(
		private->item->wia_props,
		1 /* cpspec */,
		&propspec,
		&propvariant,
		WIA_IPA_FIRST
	);
	PropVariantClear(&propvariant);
	lis_log_debug(
		"%s->WriteMultiple(%lu (%s)): 0x%lX",
		private->item->parent.name, private->wia2lis->wia.id,
		private->wia2lis->lis.name, hr
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_warning(
			"Failed to set property %lu, %s on item %s:"
			" 0x%lX -> 0x%X (%s)",
			private->wia2lis->wia.id, private->wia2lis->lis.name,
			private->item->parent.name,
			hr, err, lis_strerror(err)
		);
		return err;
	}

	lis_log_info(
		"%s->%s->set_value() successful",
		private->item->parent.name,
		private->wia2lis->lis.name
	);
	return LIS_OK;

}


static enum lis_error wiall_item_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	)
{
	struct wiall_item_private *private = WIALL_ITEM_PRIVATE(self);
	enum lis_error err;

	lis_log_info("%s->scan_start() ...", private->parent.name);

	if (private->scan != NULL) {
		wia_transfer_free(private->scan);
		private->scan = NULL;
	}

	err = wia_transfer_new(
		private->wia_item,
		private->wia_props,
		&private->scan
	);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s->scan_start() failed: 0x%X, %s",
			private->parent.name,
			err, lis_strerror(err)
		);
		private->scan = NULL;
		return err;
	}
	lis_log_info(
		"%s->scan_start() successful",
		private->parent.name
	);

	*session = wia_transfer_get_scan_session(private->scan);
	return err;
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
