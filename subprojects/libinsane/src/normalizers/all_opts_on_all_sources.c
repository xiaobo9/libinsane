#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#define NAME "normalizer_all_opts_on_all_sources"


struct opts_impl
{
	struct lis_api parent;
	struct lis_api *wrapped;
};
#define LIS_OPTS_PRIVATE(impl) ((struct opts_impl *)(impl));


struct opts_item
{
	struct lis_item parent;
	struct lis_item *wrapped;
	struct opts_item *dev;

	struct opts_item *children;
	struct opts_item **children_ptrs;

	struct lis_option_descriptor **opts;
};
#define LIS_OPTS_ITEM_PRIVATE(item) ((struct opts_item *)(item))


static void opts_cleanup(struct lis_api *impl);
static enum lis_error opts_list_devices(
	struct lis_api *impl, enum lis_device_locations locs, struct lis_device_descriptor ***dev_infos
);
static enum lis_error opts_get_device(struct lis_api *impl, const char *dev_id, struct lis_item **item);


static const struct lis_api g_impl_template = {
	.cleanup = opts_cleanup,
	.list_devices = opts_list_devices,
	.get_device = opts_get_device,
};


static enum lis_error opts_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error opts_dev_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error opts_source_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error opts_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void opts_dev_close(struct lis_item *self);
static void opts_source_close(struct lis_item *self);


static const struct lis_item g_dev_template = {
	.get_children = opts_get_children,
	.get_options = opts_dev_get_options,
	.scan_start = opts_scan_start,
	.close = opts_dev_close,
};

static const struct lis_item g_source_template = {
	.get_children = opts_get_children,
	.get_options = opts_source_get_options,
	.scan_start = opts_scan_start,
	.close = opts_source_close,
};


enum lis_error lis_api_normalizer_all_opts_on_all_sources(struct lis_api *to_wrap, struct lis_api **impl)
{
	struct opts_impl *private;

	private = calloc(1, sizeof(struct opts_impl));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	memcpy(&private->parent, &g_impl_template, sizeof(private->parent));
	private->wrapped = to_wrap;
	private->parent.base_name = to_wrap->base_name;
	*impl = &private->parent;

	return LIS_OK;
}


static void opts_cleanup(struct lis_api *impl)
{
	struct opts_impl *private = LIS_OPTS_PRIVATE(impl);
	private->wrapped->cleanup(private->wrapped);
	FREE(private);
}


static enum lis_error opts_list_devices(
		struct lis_api *impl, enum lis_device_locations locs, struct lis_device_descriptor ***dev_infos
	)
{
	struct opts_impl *private = LIS_OPTS_PRIVATE(impl);
	return private->wrapped->list_devices(private->wrapped, locs, dev_infos);
}


static enum lis_error opts_get_device(struct lis_api *impl, const char *dev_id, struct lis_item **item)
{
	enum lis_error err;
	struct opts_item *item_private;
	struct opts_impl *private = LIS_OPTS_PRIVATE(impl);

	item_private = calloc(1, sizeof(struct opts_item));
	if (item_private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	err = private->wrapped->get_device(private->wrapped, dev_id, &item_private->wrapped);
	if (LIS_IS_ERROR(err)) {
		FREE(item_private);
		lis_log_debug("get_device(%s) failed: 0x%X, %s",
			dev_id, err, lis_strerror(err)
		);
		return err;
	}

	memcpy(&item_private->parent, &g_dev_template, sizeof(item_private->parent));
	item_private->parent.name = item_private->wrapped->name;
	item_private->parent.type = item_private->wrapped->type;
	*item = &item_private->parent;

	return err;
}


static void free_options(struct opts_item *private)
{
	FREE(private->opts);
}


static void free_children(struct opts_item *private)
{
	int i;
	if (private->children_ptrs != NULL) {
		for (i = 0 ; private->children_ptrs[i] != NULL ; i++) {
			free_children(private->children_ptrs[i]);
		}
	}
	free_options(private);
	FREE(private->children);
	FREE(private->children_ptrs);
}


static void item_close(struct opts_item *private)
{
	free_children(private);
	private->wrapped->close(private->wrapped);
	FREE(private);
}


static enum lis_error opts_get_children(struct lis_item *self, struct lis_item ***out_children)
{
	struct opts_item *private = LIS_OPTS_ITEM_PRIVATE(self);
	struct lis_item **children;
	enum lis_error err;
	int i, nb_children;

	err = private->wrapped->get_children(private->wrapped, &children);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("Failed to get children items: 0x%X, %s",
			err, lis_strerror(err));
		return err;
	}

	free_children(private);

	for (nb_children = 0 ; children[nb_children] != NULL ; nb_children++) { }

	if (nb_children == 0) {
		*out_children = children;
		return err;
	}

	private->children = calloc(nb_children, sizeof(struct opts_item));
	private->children_ptrs = calloc(nb_children + 1, sizeof(struct opts_item *));
	if (private->children == NULL || private->children_ptrs == NULL) {
		free_children(private);
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (i = 0 ; i < nb_children ; i++) {
		memcpy(&private->children[i].parent, &g_source_template,
			sizeof(private->children[i].parent));
		private->children[i].parent.name = children[i]->name;
		private->children[i].parent.type = children[i]->type;
		private->children[i].wrapped = children[i];
		private->children[i].dev = private;
		private->children_ptrs[i] = &private->children[i];
	}
	*out_children = (struct lis_item **)private->children_ptrs;
	return err;
}


static enum lis_error opts_dev_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	struct opts_item *private = LIS_OPTS_ITEM_PRIVATE(self);
	enum lis_error err;

	/* root item --> return options as is */
	err = private->wrapped->get_options(private->wrapped, descs);
	if (LIS_IS_ERROR(err)) {
		private->opts = NULL;
		return err;
	}
	return err;
}


static enum lis_error opts_source_get_options(
		struct lis_item *self, struct lis_option_descriptor ***out_descs
	)
{
	struct opts_item *private = LIS_OPTS_ITEM_PRIVATE(self);
	enum lis_error err;
	struct lis_option_descriptor **dev_opts;
	struct lis_option_descriptor **source_opts;
	int dev_opt_idx, source_opt_idx;
	int nb_opts, nb_source_opts, nb_root_opts;

	/* we need to figure out how many option descriptor pointers
	 * we must allocate.
	 */

	err = private->wrapped->get_options(private->wrapped, &source_opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("Failed to get options from child item [%s]: 0x%X, %s",
			self->name, err, lis_strerror(err));
		return err;
	}

	for (source_opt_idx = 0 ; source_opts[source_opt_idx] != NULL ; source_opt_idx++) { }
	nb_opts = source_opt_idx;
	nb_source_opts = source_opt_idx;

	err = private->dev->wrapped->get_options(private->dev->wrapped, &dev_opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("Failed to get options from root item: 0x%X, %s",
			err, lis_strerror(err));
		return err;
	}

	for (dev_opt_idx = 0, nb_root_opts = 0 ; dev_opts[dev_opt_idx] != NULL ; dev_opt_idx++, nb_root_opts++) {
		for (source_opt_idx = 0 ; source_opts[source_opt_idx] != NULL ; source_opt_idx++) {
			if (strcasecmp(dev_opts[dev_opt_idx]->name,
					source_opts[source_opt_idx]->name) == 0) {
				break;
			}
		}
		if (source_opts[source_opt_idx] != NULL) {
			lis_log_info("Option '%s' from root item already present on child item '%s'",
				dev_opts[dev_opt_idx]->name, self->name);
		} else {
			nb_opts++;
		}
	}

	free_options(private);

	if (nb_opts == 0) {
		*out_descs = source_opts;
		return LIS_OK;
	}

	lis_log_info(
		"Number of options, on root item: %d, on source item: %d",
		nb_root_opts, nb_source_opts
	);

	/* now we can actually copy the options.
	*/
	private->opts = calloc(nb_opts + 1, sizeof(struct lis_option_descriptor *));
	if (private->opts == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	nb_opts = 0;
	if (nb_source_opts > 0) {
		/* WORKAROUND(Jflesch):
		 * Theorically, it's not possible to call get_options() on
		 * both the root item and the child item and expect both
		 * results to remain valid at the same time (the second call
		 * to get_options() may invalidate the result).
		 *
		 * This is an API problem.
		 *
		 * However, we know that internally:
		 * - Sane / Twain: maintain an option list only for the root item.
		 *   normalizer/source_nodes always return an empty list of
		 *   options, always valid.
		 * - WIA: the WIA implementation maintains both child and root
		 *   options separately
		 *
		 * So this should be safe. Of course, it relies on internal
		 * details of implementation, so it's baaaaddddd.
		 */

		for (source_opt_idx = 0 ; source_opts[source_opt_idx] != NULL ; source_opt_idx++) {
			private->opts[nb_opts] = source_opts[source_opt_idx];
			nb_opts++;
		}
	}

	for (dev_opt_idx = 0 ; dev_opts[dev_opt_idx] != NULL ; dev_opt_idx++) {
		for (source_opt_idx = 0 ; source_opts[source_opt_idx] != NULL ; source_opt_idx++) {
			if (strcasecmp(dev_opts[dev_opt_idx]->name,
					source_opts[source_opt_idx]->name) == 0) {
				break;
			}
		}
		if (source_opts[source_opt_idx] == NULL) {
			lis_log_info("Adding option '%s' from root item to child item '%s'",
				dev_opts[dev_opt_idx]->name, self->name);
			private->opts[nb_opts] = dev_opts[dev_opt_idx];
			nb_opts++;
		}
	}

	*out_descs = private->opts;
	return LIS_OK;
}


static enum lis_error opts_scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	struct opts_item *private = LIS_OPTS_ITEM_PRIVATE(self);
	return private->wrapped->scan_start(private->wrapped, session);
}


static void opts_dev_close(struct lis_item *self)
{
	struct opts_item *private = LIS_OPTS_ITEM_PRIVATE(self);
	item_close(private);
}


static void opts_source_close(struct lis_item *self)
{
	LIS_UNUSED(self);
	/* no-op */
}
