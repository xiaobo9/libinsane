#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/dumb.h>
#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/util.h>


static void dumb_cleanup(struct lis_api *impl);
static enum lis_error dumb_list_devices(
	struct lis_api *impl, enum lis_device_locations, struct lis_device_descriptor ***dev_infos
);
static enum lis_error dumb_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item
);

struct lis_dumb_option {
	struct lis_option_descriptor parent;

	int has_value;
	union lis_value default_value;
	union lis_value value;
};
#define LIS_DUMB_OPTION(opt) ((struct lis_dumb_option *)(opt))

struct lis_dumb_item {
	struct lis_item base;
	struct lis_dumb_private *impl;

	struct {
		struct lis_dumb_option source;
		struct lis_option_descriptor *ptrs[2];
	} opts;

	const char *dev_id;
};
#define LIS_DUMB_ITEM(impl) ((struct lis_dumb_item *)(impl))

struct lis_dumb_private {
	struct lis_api base;

	enum lis_error list_devices_ret;
	struct lis_device_descriptor **descs;

	enum lis_error get_device_ret;
	struct lis_dumb_item **devices;

	struct {
		union lis_value *source_constraint;
	} opts;
};
#define LIS_DUMB_PRIVATE(impl) ((struct lis_dumb_private *)(impl))


static enum lis_error dumb_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error dumb_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error dumb_get_scan_parameters(
	struct lis_item *self, struct lis_scan_parameters *parameters
);
static enum lis_error dumb_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void dumb_close(struct lis_item *self);


static struct lis_item g_dumb_item_template = {
	.name = "dumb-o-jet",
	.type = LIS_ITEM_DEVICE,
	.get_children = dumb_get_children,
	.get_options = dumb_get_options,
	.get_scan_parameters = dumb_get_scan_parameters,
	.scan_start = dumb_scan_start,
	.close = dumb_close,
};

static struct lis_api g_dumb_api_template = {
	.base_name = NULL,
	.cleanup = dumb_cleanup,
	.list_devices = dumb_list_devices,
	.get_device = dumb_get_device,
};

static struct lis_device_descriptor *g_dumb_default_devices[] = { NULL };
static struct lis_item *g_dumb_default_children[] = { NULL };

static void dumb_cleanup_descs(struct lis_device_descriptor **descs)
{
	int i;

	if (descs == g_dumb_default_devices) {
		return;
	}

	for (i = 0 ; descs[i] != NULL ; i++) {
		free(descs[i]->dev_id);
		free(descs[i]);
	}

	free(descs);
}

static void dumb_cleanup_devices(struct lis_dumb_item **devs)
{
	int i;
	if (devs == NULL) {
		return;
	}

	for (i = 0 ; devs[i] != NULL ; i++) {
		free(devs[i]);
	}

	free(devs);
}

static void dumb_cleanup(struct lis_api *self)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	free((void*)self->base_name);

	dumb_cleanup_descs(private->descs);
	dumb_cleanup_devices(private->devices);
	FREE(private->opts.source_constraint);
	free(private);
}


static enum lis_error dumb_list_devices(
		struct lis_api *self, enum lis_device_locations locations,
		struct lis_device_descriptor ***dev_infos
	)
{
	LIS_UNUSED(locations);

	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	if (LIS_IS_OK(private->list_devices_ret)) {
		*dev_infos = private->descs;
	} else {
		*dev_infos = NULL;
	}
	return private->list_devices_ret;
}

static enum lis_error dumb_get_device(
		struct lis_api *self, const char *dev_id, struct lis_item **item
	)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	int i;

	if (private->devices == NULL) {
		lis_log_error("[dumb] get_device() called when no device has been set"
				"; shouldn't happen");
		return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
	}

	if (LIS_IS_OK(private->get_device_ret)) {
		for (i = 0 ; private->devices[i] != NULL ; i++) {
			if (strcmp(dev_id, private->devices[i]->dev_id) == 0) {
				*item = &private->devices[i]->base;
				return LIS_OK;
			}
		}
		return LIS_ERR_INVALID_VALUE;
	} else {
		return private->get_device_ret;
	}
}


static enum lis_error dumb_get_children(struct lis_item *self, struct lis_item ***children)
{
	LIS_UNUSED(self);
	*children = g_dumb_default_children;
	return LIS_OK;
}


static void free_opt_values(struct lis_dumb_item *private)
{
	FREE(private->opts.source.value.string); /* drop const */
}


static enum lis_error dumb_opt_get_value(struct lis_option_descriptor *self, union lis_value *out_value)
{
	struct lis_dumb_option *private = LIS_DUMB_OPTION(self);
	if (private->has_value) {
		memcpy(out_value, &private->value, sizeof(*out_value));
	} else {
		memcpy(out_value, &private->default_value, sizeof(*out_value));
	}
	return LIS_OK;
}


static enum lis_error dumb_opt_set_value(struct lis_option_descriptor *self,
		union lis_value value, int *set_flags)
{
	struct lis_dumb_option *private = LIS_DUMB_OPTION(self);

	switch(self->value.type) {
		case LIS_TYPE_BOOL:
		case LIS_TYPE_INTEGER:
		case LIS_TYPE_DOUBLE:
		case LIS_TYPE_IMAGE_FORMAT:
			memcpy(&private->value, &value, sizeof(private->value));
			break;
		case LIS_TYPE_STRING:
			FREE(private->value.string);
			private->value.string = strdup(value.string);
			break;
	}
	private->has_value = 1;

	*set_flags = LIS_SET_FLAG_MUST_RELOAD_PARAMS;
	return LIS_OK;
}


static enum lis_error dumb_get_options(
		struct lis_item *self, struct lis_option_descriptor ***out_descs
	)
{
	struct lis_dumb_item *private = LIS_DUMB_ITEM(self);
	static struct lis_dumb_option opt_source_template = {
		.parent = {
			.name = "source",
			.title = "source title",
			.desc = "source desc",
			.capabilities = LIS_CAP_SW_SELECT,
			.value = {
				.type = LIS_TYPE_STRING,
				.unit = LIS_UNIT_NONE,
			},
			.constraint = {
				.type = LIS_CONSTRAINT_LIST,
				// .possible.list =
			},
			.fn = {
				.get_value = dumb_opt_get_value,
				.set_value = dumb_opt_set_value,
			},
		},
		.default_value.string = "flatbed",
		.value.string = NULL,
	};

	int nb;

	free_opt_values(private);

	memcpy(&private->opts.source, &opt_source_template, sizeof(private->opts.source));
	private->opts.ptrs[0] = &private->opts.source.parent;

	for (nb = 0 ; private->impl->opts.source_constraint[nb].string != NULL ; nb++) {
	}
	private->opts.source.parent.constraint.possible.list.nb_values = nb;
	private->opts.source.parent.constraint.possible.list.values = private->impl->opts.source_constraint;

	*out_descs = private->opts.ptrs;
	return LIS_OK;
}


static enum lis_error dumb_get_scan_parameters(
		struct lis_item *self, struct lis_scan_parameters *parameters
	)
{
	static struct lis_scan_parameters template = {
		.format = LIS_IMG_FORMAT_RAW_RGB_24,
		.width = 256,
		.height = 256,
		.image_size = 256 * 256 * 3,
	};

	LIS_UNUSED(self);

	memcpy(parameters, &template, sizeof(*parameters));
	return LIS_OK;
}


static enum lis_error dumb_scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	LIS_UNUSED(self);
	LIS_UNUSED(session);
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static void dumb_close(struct lis_item *self)
{
	struct lis_dumb_item *private = LIS_DUMB_ITEM(self);
	free_opt_values(private);
}


enum lis_error lis_api_dumb(struct lis_api **out_impl, const char *name)
{
	struct lis_dumb_private *private;

	private = calloc(1, sizeof(struct lis_dumb_private));
	memcpy(&private->base, &g_dumb_api_template, sizeof(private->base));
	private->base.base_name = strdup(name);

	private->list_devices_ret = LIS_OK;
	private->descs = g_dumb_default_devices;
	private->get_device_ret = LIS_OK;

	*out_impl = &private->base;
	return LIS_OK;
}


void lis_dumb_set_nb_devices(struct lis_api *self, int nb_devices)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	int i;
	struct lis_dumb_item *item;

	private->descs = calloc(nb_devices + 1, sizeof(struct lis_device_descriptor *));
	for (i = 0 ; i < nb_devices ; i++) {
		private->descs[i] = calloc(1, sizeof(struct lis_device_descriptor));
		private->descs[i]->impl = &private->base;
		asprintf(&private->descs[i]->dev_id, "dumb dev%d", i);
		private->descs[i]->vendor = "Microsoft";
		private->descs[i]->model = "Bugware";
		private->descs[i]->type = NULL;
	}

	private->devices = calloc(nb_devices + 1, sizeof(struct lis_dumb_item *));
	for (i = 0 ; i < nb_devices ; i++) {
		item = calloc(1, sizeof(struct lis_dumb_item));
		memcpy(&item->base, &g_dumb_item_template, sizeof(item->base));
		item->impl = private;
		item->dev_id = private->descs[i]->dev_id;
		private->devices[i] = item;
	}
}


void lis_dumb_set_list_devices_return(struct lis_api *self, enum lis_error ret)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	private->list_devices_ret = ret;
}


void lis_dumb_set_get_device_return(struct lis_api *self, enum lis_error ret)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	private->get_device_ret = ret;
}


void lis_dumb_set_opt_source_constraint(struct lis_api *self, const char **constraint)
{
	struct lis_dumb_private *private = LIS_DUMB_PRIVATE(self);
	int nb;

	for (nb = 0 ; constraint[nb] != NULL ; nb++) {
	}

	FREE(private->opts.source_constraint);
	private->opts.source_constraint = calloc(nb + 1, sizeof(union lis_value));

	for (nb = 0 ; constraint[nb] != NULL ; nb++) {
		private->opts.source_constraint[nb].string = constraint[nb];
	}
}
