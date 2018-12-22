#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>


struct cache_opt_private {
	struct lis_option_descriptor parent;
	struct lis_option_descriptor *wrapped;

	bool has_last_value;
	union lis_value last_value;
};
#define CACHE_OPT_PRIVATE(opt) ((struct cache_opt_private *)(opt))


struct cache_item_private {
	struct lis_item parent;
	struct lis_item *wrapped;

	struct cache_item_private *children;
	struct lis_item **children_ptrs;

	struct cache_opt_private *opts;
	struct lis_option_descriptor **opts_ptrs;
};
#define CACHE_ITEM_PRIVATE(item) ((struct cache_item_private *)(item))


struct cache_impl_private {
	struct lis_api parent;
	struct lis_api *wrapped;
};
#define CACHE_IMPL_PRIVATE(item) ((struct cache_impl_private *)(impl))



static enum lis_error cache_get_value(
	struct lis_option_descriptor *self, union lis_value *value
);
static enum lis_error cache_set_value(
	struct lis_option_descriptor *self, union lis_value value,
	int *set_flags
);


static enum lis_error cache_get_children(
	struct lis_item *self, struct lis_item ***children
);
static enum lis_error cache_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error cache_scan_start(
	struct lis_item *self, struct lis_scan_session **session
);
static void cache_child_close(struct lis_item *self);
static void cache_root_close(struct lis_item *self);


static struct lis_item g_item_child_template = {
	.get_children = cache_get_children,
	.get_options = cache_get_options,
	.scan_start = cache_scan_start,
	.close = cache_child_close,
};
static struct lis_item g_item_root_template = {
	.get_children = cache_get_children,
	.get_options = cache_get_options,
	.scan_start = cache_scan_start,
	.close = cache_root_close,
};


static enum lis_error cache_list_devices(
	struct lis_api *impl, enum lis_device_locations locs,
	struct lis_device_descriptor ***dev_infos
);
static enum lis_error cache_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item
);
static void cache_cleanup(struct lis_api *impl);


static struct lis_api g_impl_template = {
	.list_devices = cache_list_devices,
	.get_device = cache_get_device,
	.cleanup = cache_cleanup,
};


static void free_last_value(struct cache_opt_private *private)
{
	if (!private->has_last_value) {
		return;
	}
	private->has_last_value = 0;
	if (private->parent.value.type != LIS_TYPE_STRING) {
		return;
	}
	FREE(private->last_value.string);
}



static void set_last_value(
		struct cache_opt_private *private, union lis_value value
	)
{
	free_last_value(private);

	memcpy(&private->last_value, &value, sizeof(private->last_value));
	if (private->parent.value.type == LIS_TYPE_STRING) {
		private->last_value.string = strdup(value.string);
		if (private->last_value.string == NULL) {
			lis_log_error("Out of memory");
			return;
		}
	}
	private->has_last_value = 1;
}


static enum lis_error cache_get_value(
		struct lis_option_descriptor *self, union lis_value *value
	)
{
	struct cache_opt_private *private = CACHE_OPT_PRIVATE(self);
	enum lis_error err;

	if (private->has_last_value) {
		lis_log_info(
			"%s->get_value(): Using cached value",
			self->name
		);
		memcpy(value, &private->last_value, sizeof(*value));
		return LIS_OK;
	} else {
		err = private->wrapped->fn.get_value(private->wrapped, value);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"%s->get_value() failed: 0x%X, %s",
				self->name, err, lis_strerror(err)
			);
			return err;
		}
		set_last_value(private, *value);
		return err;
	}
}


static enum lis_error cache_set_value(
		struct lis_option_descriptor *self, union lis_value value,
		int *set_flags
	)
{
	struct cache_opt_private *private = CACHE_OPT_PRIVATE(self);
	enum lis_error err;

	*set_flags = 0;

	err = private->wrapped->fn.set_value(
		private->wrapped, value, set_flags
	);
	free_last_value(private);
	if (LIS_IS_ERROR(err)) {
		lis_log_info(
			"%s->set_value() failed: 0x%X, %s",
			self->name, err, lis_strerror(err)
		);
		return err;
	}

	if (((*set_flags) & (
				LIS_SET_FLAG_MUST_RELOAD_OPTIONS
				| LIS_SET_FLAG_INEXACT
			))) {
		return err;
	}

	set_last_value(private, value);
	return err;
}


static void free_opts(struct cache_item_private *private)
{
	int i;

	if (private->opts_ptrs != NULL) {
		for (i = 0 ; private->opts_ptrs[i] != NULL ; i++) {
			free_last_value(&private->opts[i]);
		}

		FREE(private->opts);
		FREE(private->opts_ptrs);
	}
}


static enum lis_error cache_get_options(
		struct lis_item *self, struct lis_option_descriptor ***out_descs
	)
{
	struct lis_option_descriptor **opts;
	enum lis_error err;
	int nb_opts, i;
	struct cache_item_private *private = CACHE_ITEM_PRIVATE(self);

	if (private->opts_ptrs != NULL) {
		lis_log_info(
			"%s->get_options(): returning cached options",
			self->name
		);
		*out_descs = private->opts_ptrs;
		return LIS_OK;
	}

	err = private->wrapped->get_options(private->wrapped, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s->get_options() failed: 0x%X, %s",
			self->name, err, lis_strerror(err)
		);
		return err;
	}

	for (nb_opts = 0; opts[nb_opts] != NULL ; nb_opts++) {
	}

	lis_log_debug(
		"%s->get_options() returned %d options", self->name, nb_opts
	);

	private->opts_ptrs = calloc(
		nb_opts + 1, sizeof(struct lis_option_descriptor *)
	);
	if (private->opts_ptrs == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	if (nb_opts > 0) {
		private->opts = calloc(
			nb_opts, sizeof(struct cache_opt_private)
		);
		if (private->opts == NULL) {
			lis_log_error("Out of memory");
			FREE(private->opts_ptrs);
			return LIS_ERR_NO_MEM;
		}
	}

	for (i = 0 ; i < nb_opts ; i++) {
		memcpy(
			&private->opts[i].parent, opts[i],
			sizeof(private->opts[i].parent)
		);
		private->opts[i].wrapped = opts[i];
		private->opts[i].parent.fn.set_value = cache_set_value;
		private->opts[i].parent.fn.get_value = cache_get_value;
		private->opts_ptrs[i] = &private->opts[i].parent;
	}

	*out_descs = private->opts_ptrs;
	return LIS_OK;
}


static void close_children(struct cache_item_private *private)
{
	int i;

	free_opts(private);
	if (private->children_ptrs != NULL) {
		for (i = 0 ; private->children_ptrs[i] != NULL; i++) {
			close_children(&private->children[i]);
		}
		FREE(private->children);
		FREE(private->children_ptrs);
	}
}


static enum lis_error cache_get_children(
		struct lis_item *self, struct lis_item ***out_children
	)
{
	struct lis_item **children;
	enum lis_error err;
	int nb_children, i;
	struct cache_item_private *private = CACHE_ITEM_PRIVATE(self);

	err = private->wrapped->get_children(private->wrapped, &children);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s->get_children() failed: 0x%X, %s",
			self->name, err, lis_strerror(err)
		);
		return err;
	}

	close_children(private);

	for (nb_children = 0 ; children[nb_children] != NULL ; nb_children++) {
	}

	private->children_ptrs = calloc(
		nb_children + 1, sizeof(struct lis_item *)
	);
	if (private->children_ptrs == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	if (nb_children > 0) {
		private->children = calloc(
			nb_children, sizeof(struct cache_item_private)
		);
		if (private->children == NULL) {
			lis_log_error("Out of memory");
			FREE(private->children_ptrs);
			return LIS_ERR_NO_MEM;
		}
	}

	for (i = 0 ; children[i] != NULL ; i++) {
		memcpy(
			&private->children[i].parent, &g_item_child_template,
			sizeof(private->children[i].parent)
		);
		private->children[i].parent.name = children[i]->name;
		private->children[i].wrapped = children[i];
		private->children_ptrs[i] = &private->children[i].parent;
	}

	*out_children = private->children_ptrs;
	return LIS_OK;
}


static enum lis_error cache_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	)
{
	struct cache_item_private *private = CACHE_ITEM_PRIVATE(self);
	return private->wrapped->scan_start(private->wrapped, session);
}


static void cache_child_close(struct lis_item *self)
{
	struct cache_item_private *private = CACHE_ITEM_PRIVATE(self);
	private->wrapped->close(private->wrapped);
}


static void cache_root_close(struct lis_item *self)
{
	struct cache_item_private *private = CACHE_ITEM_PRIVATE(self);

	private->wrapped->close(private->wrapped);
	private->wrapped = NULL;

	close_children(private);
	FREE(private);
}


static enum lis_error cache_list_devices(
		struct lis_api *impl, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct cache_impl_private *private = CACHE_IMPL_PRIVATE(impl);
	return private->wrapped->list_devices(
		private->wrapped, locs, dev_infos
	);
}


static enum lis_error cache_get_device(
		struct lis_api *impl, const char *dev_id,
		struct lis_item **out_item
	)
{
	struct cache_impl_private *private = CACHE_IMPL_PRIVATE(impl);
	struct cache_item_private *item;
	enum lis_error err;

	item = calloc(1, sizeof(struct cache_item_private));
	if (item == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(
		&item->parent, &g_item_root_template,
		sizeof(item->parent)
	);

	err = private->wrapped->get_device(
		private->wrapped, dev_id, &item->wrapped
	);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get_device(%s): 0x%X, %s",
			dev_id, err, lis_strerror(err)
		);
		FREE(item);
		return err;
	}
	item->parent.name = item->wrapped->name;
	*out_item = &item->parent;
	return LIS_OK;
}


static void cache_cleanup(struct lis_api *impl)
{
	struct cache_impl_private *private = CACHE_IMPL_PRIVATE(impl);

	private->wrapped->cleanup(private->wrapped);
	FREE(private);
}


enum lis_error lis_api_workaround_cache(
		struct lis_api *to_wrap, struct lis_api **out_impl
	)
{
	struct cache_impl_private *private;

	private = calloc(1, sizeof(struct cache_impl_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(&private->parent, &g_impl_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;
	private->wrapped = to_wrap;

	*out_impl = &private->parent;
	return LIS_OK;
}
