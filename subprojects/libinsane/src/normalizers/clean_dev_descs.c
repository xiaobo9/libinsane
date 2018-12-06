#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>


static void impl_cleanup(struct lis_api *impl);
static enum lis_error impl_list_devices(
	struct lis_api *impl, enum lis_device_locations, struct lis_device_descriptor ***dev_infos
);
static enum lis_error impl_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item
);


struct lis_clean_private {
	struct lis_api parent;
	struct lis_api *wrapped;

	struct lis_device_descriptor *descs;
	struct lis_device_descriptor **descs_ptr;
};
#define LIS_CLEAN_PRIVATE(impl) ((struct lis_clean_private *)(impl))


static const struct lis_api g_api_template = {
	.cleanup = impl_cleanup,
	.list_devices = impl_list_devices,
	.get_device = impl_get_device,
};


typedef void (*filter_names_fn)(char **vendor, char **model);
static void filter_underscores(char **vendor, char **model);
static void shorten_manufacturer(char **vendor, char **model);
static void filter_manufacturer(char **vendor, char **model);


static filter_names_fn g_filter_fn[] = {
	filter_underscores,
	shorten_manufacturer,
	filter_manufacturer,
	NULL
};


static void free_descs(struct lis_clean_private *private)
{
	int i;
	if (private->descs_ptr != NULL) {
		for (i = 0 ; private->descs_ptr[i] != NULL ; i++) {
			FREE(private->descs[i].vendor);
			FREE(private->descs[i].model);
		}
	}
	FREE(private->descs);
	FREE(private->descs_ptr);
}


static void impl_cleanup(struct lis_api *impl)
{
	struct lis_clean_private *private = LIS_CLEAN_PRIVATE(impl);
	private->wrapped->cleanup(private->wrapped);
	free_descs(private);
	FREE(private);
}


static enum lis_error impl_list_devices(
		struct lis_api *impl, enum lis_device_locations loc,
		struct lis_device_descriptor ***out_dev_descs
	)
{
	struct lis_clean_private *private = LIS_CLEAN_PRIVATE(impl);
	struct lis_device_descriptor **descs;
	enum lis_error err;
	int nb_devs;
	int i;

	free_descs(private);

	err = private->wrapped->list_devices(private->wrapped, loc, &descs);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	for (nb_devs = 0 ; descs[nb_devs] != NULL ; nb_devs++) { }

	if (nb_devs == 0) {
		*out_dev_descs = descs;
		return LIS_OK;
	}

	private->descs = calloc(nb_devs, sizeof(struct lis_device_descriptor));
	private->descs_ptr = calloc(nb_devs + 1, sizeof(struct lis_device_descriptor *));
	if (private->descs == NULL || private->descs_ptr == NULL) {
		FREE(private->descs);
		FREE(private->descs_ptr);
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (nb_devs = 0 ; descs[nb_devs] != NULL ; nb_devs++) {
		private->descs_ptr[nb_devs] = &private->descs[nb_devs];
		memcpy(&private->descs[nb_devs], descs[nb_devs], sizeof(private->descs[nb_devs]));
		private->descs[nb_devs].vendor = strdup(private->descs[nb_devs].vendor);
		private->descs[nb_devs].model = strdup(private->descs[nb_devs].model);
		for (i = 0 ; g_filter_fn[i] != NULL ; i++) {
			g_filter_fn[i](
				&(private->descs[nb_devs].vendor),
				&(private->descs[nb_devs].model)
			);
		}
	}

	*out_dev_descs = private->descs_ptr;
	return LIS_OK;
}


static enum lis_error impl_get_device(
		struct lis_api *impl, const char *dev_id, struct lis_item **item
	)
{
	struct lis_clean_private *private = LIS_CLEAN_PRIVATE(impl);
	return private->wrapped->get_device(private->wrapped, dev_id, item);
}


enum lis_error lis_api_normalizer_clean_dev_descs(struct lis_api *to_wrap, struct lis_api **impl)
{
	struct lis_clean_private *private;

	private = calloc(1, sizeof(struct lis_clean_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(&private->parent, &g_api_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;
	private->wrapped = to_wrap;
	*impl = &private->parent;
	return LIS_OK;
}


static inline void _filter_underscores(char *str)
{
	int i;
	for (i = 0 ; str[i] != '\0' ; i++) {
		if (str[i] == '_') {
			str[i] = ' ';
		}
	}
}

static void filter_underscores(char **vendor, char **model)
{
	_filter_underscores(*vendor);
	_filter_underscores(*model);
}


static void shorten_manufacturer(char **vendor, char **model)
{
	static const struct {
		const char *original;
		const char *replacement;
	} replacements[] = {
		{ .original = "hewlett-packard", .replacement = "HP" },
		{ .original = "hewlett packard", .replacement = "HP" },
		{ .original = NULL }
	};
	char *ptr;
	int i;

	LIS_UNUSED(model);

	for (i = 0 ; replacements[i].original != NULL ; i++) {
		if (strcasecmp(*vendor, replacements[i].original) == 0) {
			ptr = strdup(replacements[i].replacement);
			if (ptr == NULL) {
				lis_log_error("Out of memory");
				return;
			}
			FREE(*vendor);
			*vendor = ptr;
		}
	}
}


static void filter_manufacturer(char **vendor, char **model)
{
	char *ptr;
	int offset = 0;

	if (strncasecmp(*vendor, *model, strlen(*vendor)) == 0) {
		offset += strlen(*vendor);
	}
	if ((*model)[offset] == ' ') {
		offset += 1;
	}

	if (offset == 0) {
		return;
	}

	ptr = strdup(*model + offset);
	if (ptr == NULL) {
		lis_log_error("Out of memory");
		return;
	}
	FREE(*model);
	*model = ptr;
}
