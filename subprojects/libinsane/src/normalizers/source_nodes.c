#include <stdio.h>
#include <string.h>

#include <libinsane/constants.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>


static void lis_sn_cleanup(struct lis_api *impl);
static enum lis_error lis_sn_list_devices(
	struct lis_api *impl, enum lis_device_locations locations,
	struct lis_device_descriptor ***dev_infos
);
static enum lis_error lis_sn_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item
);


struct lis_sn_api_private
{
	struct lis_api parent;
	struct lis_api *wrapped;
};
#define LIS_SN_API_PRIVATE(impl) ((struct lis_sn_api_private *)(impl))


struct lis_sn_item_private
{
	struct lis_item item;
	struct lis_sn_device_private *device;

	struct
	{
		const char *name;
		union lis_value value;
	} opt;
};
#define LIS_SN_ITEM_PRIVATE(item) ((struct lis_sn_item_private *)(item))


struct lis_sn_device_private
{
	struct lis_sn_item_private item;
	struct lis_sn_api_private *private;

	struct lis_item *wrapped;
	int nb_sources;
	struct lis_item **source_ptrs;
	struct lis_sn_item_private *sources;

	int scan_running;
	struct lis_sn_scan_session_private *scan_session;
};
#define LIS_SN_DEVICE_PRIVATE(item) ((struct lis_sn_device_private *)(item))


struct lis_sn_scan_session_private
{
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_sn_device_private *device;
};
#define LIS_SN_SCAN_SESSION_PRIVATE(session) ((struct lis_sn_scan_session_private *)(session))


static enum lis_error lis_sn_get_scan_parameters(
	struct lis_scan_session *session,
	struct lis_scan_parameters *params
);
static int lis_sn_end_of_feed(struct lis_scan_session *session);
static int lis_sn_end_of_page(struct lis_scan_session *session);
static enum lis_error lis_sn_scan_read(
	struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
);
static void lis_sn_cancel(struct lis_scan_session *session);

static const struct lis_scan_session g_sn_scan_session_template = {
	.get_scan_parameters = lis_sn_get_scan_parameters,
	.end_of_feed = lis_sn_end_of_feed,
	.end_of_page = lis_sn_end_of_page,
	.scan_read = lis_sn_scan_read,
	.cancel = lis_sn_cancel,
};


static enum lis_error lis_sn_dev_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);

static enum lis_error lis_sn_scan_start(struct lis_item *self, struct lis_scan_session **session);
static enum lis_error lis_sn_dev_get_children(struct lis_item *self, struct lis_item ***children);
static void lis_sn_dev_close(struct lis_item *self);


static const struct lis_item g_sn_dev_template = {
	.name = NULL,
	.type = LIS_ITEM_UNIDENTIFIED,
	.get_children = lis_sn_dev_get_children,
	.get_options = lis_sn_dev_get_options,
	/* we have to let scan_start() work on the root node
	 * as well, otherwise the normalizer 'min_one_source' can't work
	 */
	.scan_start = lis_sn_scan_start,
	.close = lis_sn_dev_close,
};


static enum lis_error lis_sn_src_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error lis_sn_src_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static void lis_sn_src_close(struct lis_item *self);


static const struct lis_item g_sn_source_template = {
	.name = NULL,
	.type = LIS_ITEM_UNIDENTIFIED,
	.get_children = lis_sn_src_get_children,
	.get_options = lis_sn_src_get_options,
	.scan_start = lis_sn_scan_start,
	.close = lis_sn_src_close,
};


static const struct lis_api g_sn_api_template = {
	.cleanup = lis_sn_cleanup,
	.list_devices = lis_sn_list_devices,
	.get_device = lis_sn_get_device,
};

static void lis_sn_cleanup(struct lis_api *impl)
{
	struct lis_sn_api_private *private = LIS_SN_API_PRIVATE(impl);
	private->wrapped->cleanup(private->wrapped);
	free(private);
}

static enum lis_error lis_sn_list_devices(
		struct lis_api *impl, enum lis_device_locations locations,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct lis_sn_api_private *private = LIS_SN_API_PRIVATE(impl);
	return private->wrapped->list_devices(private->wrapped, locations, dev_infos);
}


static enum lis_error lis_sn_src_get_children(struct lis_item *self, struct lis_item ***children)
{
	static struct lis_item *g_empty_child_list[] = { NULL };
	LIS_UNUSED(self);
	*children = g_empty_child_list;
	return LIS_OK;
}


static enum lis_error lis_sn_dev_get_children(struct lis_item *self, struct lis_item ***children)
{
	struct lis_sn_item_private *private = LIS_SN_ITEM_PRIVATE(self);
	struct lis_sn_device_private *device = private->device;
	struct lis_option_descriptor **opts = NULL;
	enum lis_error err;
	enum lis_error get_children_err;
	int opt_idx;
	int source_idx;

	if (device->source_ptrs != NULL) {
		/* already built the sources */
		*children = device->source_ptrs;
		return LIS_OK;
	}

	get_children_err = device->wrapped->get_children(device->wrapped, children);
	if (LIS_IS_OK(get_children_err) && *children[0] != NULL) {
		lis_log_info("Wrapped implementation already provides child sources");
		return get_children_err;
	}

	err = device->wrapped->get_options(device->wrapped, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("wrapped->get_options() failed: 0x%X, %s", err, lis_strerror(err));
		return err;
	}

	for (opt_idx = 0 ; opts[opt_idx] != NULL ; opt_idx++) {
		if (strcasecmp(opts[opt_idx]->name, OPT_NAME_SOURCE) == 0) {
			break;
		} else if (strcasecmp(opts[opt_idx]->name, OPT_NAME_FEEDER_ENABLED) == 0) {
			break;
		}
	}
	if (opts[opt_idx] == NULL) {
		lis_log_warning(
			"Failed to get child items from wrapped implementation"
			" + no option \"source\"/\"feeder_enabled\" found"
		);
		return get_children_err;
	}

	if (opts[opt_idx]->constraint.type != LIS_CONSTRAINT_LIST
		|| opts[opt_idx]->constraint.possible.list.nb_values <= 0) {
		lis_log_warning(
			"Failed to get child items from wrapped implementation"
			" + option \"source\"/\"feeder_enabled\" doesn't have"
			" expected types (%d:%d:%d)",
			opts[opt_idx]->value.type, opts[opt_idx]->constraint.type,
			opts[opt_idx]->constraint.possible.list.nb_values
		);
		return get_children_err;
	}

	device->nb_sources = opts[opt_idx]->constraint.possible.list.nb_values;
	lis_log_info("Generating %d sources from constraint of option %s",
		device->nb_sources, OPT_NAME_SOURCE);

	device->sources = calloc(device->nb_sources, sizeof(struct lis_sn_item_private));
	device->source_ptrs = calloc(device->nb_sources + 1, sizeof(struct lis_item *));
	if (device->sources == NULL || device->source_ptrs == NULL) {
		device->nb_sources = 0;
		FREE(device->sources);
		FREE(device->source_ptrs);
		return LIS_ERR_NO_MEM;
	}

	for (source_idx = 0 ; source_idx < device->nb_sources ; source_idx++) {
		memcpy(&device->sources[source_idx].item, &g_sn_source_template,
			sizeof(device->sources[source_idx].item));

		device->sources[source_idx].opt.name = strdup(opts[opt_idx]->name);
		// TODO(Jflesch): Out of memory

		if (opts[opt_idx]->value.type == LIS_TYPE_STRING) {
			device->sources[source_idx].item.name =
				strdup(opts[opt_idx]->constraint.possible.list.values[source_idx].string);
			device->sources[source_idx].opt.value.string = device->sources[source_idx].item.name;
		} else {
			memcpy(
				&device->sources[source_idx].opt.value,
				&opts[opt_idx]->constraint.possible.list.values[source_idx],
				sizeof(device->sources[source_idx].opt.value)
			);
			device->sources[source_idx].item.name = strdup(
				opts[opt_idx]->constraint.possible.list.values[source_idx].boolean
				? OPT_VALUE_SOURCE_ADF : OPT_VALUE_SOURCE_FLATBED
			);
		}

		device->source_ptrs[source_idx] = &device->sources[source_idx].item;
		device->sources[source_idx].device = device;
	}

	*children = device->source_ptrs;
	return LIS_OK;
}


static enum lis_error lis_sn_dev_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	struct lis_sn_item_private *private = LIS_SN_ITEM_PRIVATE(self);
	return private->device->wrapped->get_options(private->device->wrapped, descs);
}


static enum lis_error set_source(struct lis_sn_item_private *private)
{
	struct lis_option_descriptor **opts = NULL;
	int source_opt_idx;
	enum lis_error err;
	int set_flags;

	if (private == &private->device->item) {
		lis_log_info("Scanning on the root node --> cannot set source");
		return LIS_OK;
	}

	lis_log_info("Setting source to '%s'", private->item.name);
	err = private->device->wrapped->get_options(private->device->wrapped, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("wrapped->get_options() failed: 0x%X, %s", err, lis_strerror(err));
		return err;
	}

	for (source_opt_idx = 0 ; opts[source_opt_idx] != NULL ; source_opt_idx++) {
		if (strcasecmp(opts[source_opt_idx]->name, private->opt.name) == 0) {
			break;
		}
	}
	if (opts[source_opt_idx] == NULL) {
		lis_log_error(
			"wrapped->get_options() didn't return the option '%s'",
			private->opt.name
		);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	err = opts[source_opt_idx]->fn.set_value(
		opts[source_opt_idx],
		private->opt.value,
		&set_flags
	);
	if (LIS_IS_OK(err)) {
		lis_log_info("Source set to '%s'", private->item.name);
	} else {
		if (!LIS_OPT_IS_READABLE(opts[source_opt_idx])
				|| !LIS_OPT_IS_WRITABLE(opts[source_opt_idx])) {
			// XXX(Jflesch): Sane + Canon LiDE 220 (genesys)
			// https://openpaper.work/en-us/scanner_db/report/318/
			// We have 2 possible sources, the option is SW_SELECT,
			// but inactive, so we can't select any ...
			lis_log_warning(
				"Failed to set source: 0x%X, %s."
				" Option is inactive/read-only so we will try to keep going anyway.",
				err, lis_strerror(err)
			);
			return LIS_OK;
		}

		lis_log_error("Failed to set source: 0x%X, %s", err, lis_strerror(err));
	}
	return err;
}


static enum lis_error lis_sn_src_get_options(
		struct lis_item *self, struct lis_option_descriptor ***out_descs
	)
{
	enum lis_error err;
	struct lis_sn_item_private *private = LIS_SN_ITEM_PRIVATE(self);
	static const struct lis_option_descriptor *descs[] = { NULL };

	// XXX(Jflesch): HP drivers + sane backend 'net' (+ difference of
	// versions):
	// This combination is very sensitive: Source must be the first option
	// set. This should be a good time to set it.
	err = set_source(private);
	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
			"setting source has failed --> scan_start() failed:"
			" 0x%x, %s",
			err, lis_strerror(err)
		);
	}

	// do not return any option ; that would be redundant with normalizer
	// 'all_opts_on_all_sources'
	*out_descs = (struct lis_option_descriptor **)descs;
	return LIS_OK;
}


static enum lis_error lis_sn_scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	struct lis_sn_item_private *private_item = LIS_SN_ITEM_PRIVATE(self);
	struct lis_sn_scan_session_private *private_session;
	enum lis_error err;

	if (private_item->device->scan_running) {
		lis_log_error("scan_start() called while a scan session is already running");
		return LIS_ERR_DEVICE_BUSY;
	}
	FREE(private_item->device->scan_session);

	err = set_source(private_item);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("setting source has failed --> scan_start() failed: 0x%x, %s",
				err, lis_strerror(err));
		return err;
	}

	private_session = calloc(1, sizeof(struct lis_sn_scan_session_private));
	if (private_session == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	err = private_item->device->wrapped->scan_start(private_item->device->wrapped, &private_session->wrapped);
	if (LIS_IS_ERROR(err)) {
		FREE(private_session);
		lis_log_error("Failed to set source: 0x%X, %s", err, lis_strerror(err));
		return err;
	}

	memcpy(&private_session->parent, &g_sn_scan_session_template, sizeof(private_session->parent));
	private_session->device = private_item->device;

	private_item->device->scan_running = 1;
	private_item->device->scan_session = private_session;

	*session = &private_session->parent;
	return err;
}


static void lis_sn_src_close(struct lis_item *self)
{
	LIS_UNUSED(self);
}


static void lis_sn_dev_close(struct lis_item *self)
{
	struct lis_sn_device_private *private = LIS_SN_DEVICE_PRIVATE(self);
	int i;

	if (private->source_ptrs != NULL) {
		for (i = 0 ; i < private->nb_sources ; i++) {
			FREE(private->sources[i].opt.name);
			FREE(private->sources[i].item.name);
		}
	}

	FREE(private->source_ptrs);
	FREE(private->sources);
	FREE(private->scan_session);
	private->wrapped->close(private->wrapped);
	free(private);
}


static enum lis_error lis_sn_get_device(
		struct lis_api *impl, const char *dev_id, struct lis_item **out_item
	)
{
	struct lis_sn_api_private *api_private = LIS_SN_API_PRIVATE(impl);
	struct lis_sn_device_private *dev_private;
	enum lis_error err;

	dev_private = calloc(1, sizeof(struct lis_sn_device_private));
	if (dev_private == NULL) {
		return LIS_ERR_NO_MEM;
	}
	dev_private->private = api_private;
	dev_private->item.device = dev_private;

	err = api_private->wrapped->get_device(api_private->wrapped, dev_id, &dev_private->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("wrapped->get_device() failed: 0x%X, %s", err, lis_strerror(err));
		free(dev_private);
		return err;
	}

	memcpy(&dev_private->item.item, &g_sn_dev_template, sizeof(dev_private->item.item));
	dev_private->item.item.name = dev_private->wrapped->name;
	dev_private->item.item.type = dev_private->wrapped->type;
	*out_item = &dev_private->item.item;

	return err;
}


enum lis_error lis_api_normalizer_source_nodes(struct lis_api *to_wrap, struct lis_api **api)
{
	struct lis_sn_api_private *private = calloc(1, sizeof(struct lis_sn_api_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	memcpy(&private->parent, &g_sn_api_template, sizeof(struct lis_api));
	private->parent.base_name = to_wrap->base_name;
	private->wrapped = to_wrap;

	*api = &private->parent;
	return LIS_OK;
}


static enum lis_error lis_sn_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	struct lis_sn_scan_session_private *private = \
		LIS_SN_SCAN_SESSION_PRIVATE(self);
	return private->wrapped->get_scan_parameters(private->wrapped, params);
}


static int lis_sn_end_of_feed(struct lis_scan_session *session)
{
	struct lis_sn_scan_session_private *private = LIS_SN_SCAN_SESSION_PRIVATE(session);
	int r;
	r = private->wrapped->end_of_feed(private->wrapped);
	if (r) {
		private->device->scan_running = 0;
	}
	return r;
}


static int lis_sn_end_of_page(struct lis_scan_session *session)
{
	struct lis_sn_scan_session_private *private = LIS_SN_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->end_of_page(private->wrapped);
}


static enum lis_error lis_sn_scan_read(
		struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
	)
{
	struct lis_sn_scan_session_private *private = LIS_SN_SCAN_SESSION_PRIVATE(session);
	enum lis_error err = private->wrapped->scan_read(private->wrapped, out_buffer, buffer_size);
	if (LIS_IS_ERROR(err)) {
		private->device->scan_running = 0;
	}
	return err;
}


static void lis_sn_cancel(struct lis_scan_session *session)
{
	struct lis_sn_scan_session_private *private = LIS_SN_SCAN_SESSION_PRIVATE(session);
	private->device->scan_running = 0;
	private->wrapped->cancel(private->wrapped);
}
