#include <stdlib.h>
#include <string.h>

#include <libinsane/error.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "basewrapper.h"


struct lis_bw_impl_private {
	struct lis_api parent;
	struct lis_api *wrapped;
	const char *wrapper_name;

	struct {
		lis_bw_opt_desc_filter cb;
		void *user_data;
	} opt_desc_filter;

	struct {
		lis_bw_item_filter cb;
		void *user_data;
	} item_filter;

	struct {
		lis_bw_on_scan_start cb;
		void *user_data;
	} scan_start;

	struct {
		lis_bw_on_close_item cb;
		void *user_data;
	} on_close_item;

	struct {
		lis_bw_clean_impl cb;
		void *user_data;
	} impl_clean;

	struct lis_bw_item *roots;

	struct lis_bw_impl_private *next;
};
#define LIS_BW_IMPL_PRIVATE(impl) ((struct lis_bw_impl_private *)(impl))


struct lis_bw_item {
	struct lis_item parent;
	struct lis_item *wrapped;
	struct lis_bw_item *root;
	struct lis_bw_impl_private *impl;

	struct lis_bw_item **children;
	struct lis_bw_option_descriptor **options;

	struct lis_bw_item *next;

	void *user;
};
#define LIS_BW_ITEM(item) ((struct lis_bw_item *)(item))


struct lis_bw_option_descriptor {
	struct lis_option_descriptor parent; /*!< basewrapper modified by filter (if any) */
	struct lis_option_descriptor basewrapper; /*!< basewrapper original */
	struct lis_option_descriptor *wrapped; /*!< wrapped implementation */
	struct lis_bw_item *item;

	void *user;
	lis_bw_free_fn free_cb;
};
#define LIS_BW_OPT_DESC(opt) ((struct lis_bw_option_descriptor *)(opt))


static struct lis_bw_impl_private *g_impls = NULL;


static enum lis_error lis_bw_get_value(struct lis_option_descriptor *self, union lis_value *value);
static enum lis_error lis_bw_set_value(struct lis_option_descriptor *self, union lis_value value, int *set_flags);


static void lis_bw_cleanup(struct lis_api *impl);
static enum lis_error lis_bw_list_devices(
	struct lis_api *impl, enum lis_device_locations, struct lis_device_descriptor ***dev_infos
);
static enum lis_error lis_bw_get_device(struct lis_api *impl, const char *dev_id, struct lis_item **item);


static const struct lis_api g_bw_impl_template = {
	.cleanup = lis_bw_cleanup,
	.list_devices = lis_bw_list_devices,
	.get_device = lis_bw_get_device,
};


static enum lis_error lis_bw_item_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error lis_bw_item_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error lis_bw_item_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void lis_bw_item_root_close(struct lis_item *self);


static const struct lis_item g_bw_item_root_template = {
	.get_children = lis_bw_item_get_children,
	.get_options = lis_bw_item_get_options,
	.scan_start = lis_bw_item_scan_start,
	.close = lis_bw_item_root_close,
};


static void lis_bw_item_child_close(struct lis_item *self);

static const struct lis_item g_bw_item_child_template = {
	.get_children = lis_bw_item_get_children,
	.get_options = lis_bw_item_get_options,
	.scan_start = lis_bw_item_scan_start,
	.close = lis_bw_item_child_close,
};

static void lis_bw_cleanup(struct lis_api *in_impl)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(in_impl);
	struct lis_bw_impl_private *impl, *pimpl;

	if (private->impl_clean.cb != NULL) {
		private->impl_clean.cb(&private->parent, private->impl_clean.user_data);
	}

	/* remove itself from the global linked list */
	for (impl = g_impls, pimpl = NULL ;
			impl != NULL ;
			pimpl = impl, impl = impl->next) {
		if (impl == private) {
			if (pimpl == NULL) {
				g_impls = impl->next;
			} else {
				pimpl->next = impl->next;
			}
			break;
		}
	}

	/* then cleanup + free */
	private->wrapped->cleanup(private->wrapped);
	FREE(private);
}


static enum lis_error lis_bw_list_devices(
		struct lis_api *impl, enum lis_device_locations locations,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	return private->wrapped->list_devices(private->wrapped, locations, dev_infos);
}


static void add_root(struct lis_bw_impl_private *private, struct lis_bw_item *root)
{
	struct lis_bw_item *ptr;
	/* make sure the root isn't already in the list */
	for (ptr = private->roots ; ptr != NULL ; ptr = ptr->next) {
		if (ptr == root) {
			lis_log_warning("Root already registered: %s !", root->parent.name);
			return;
		}
	}

	root->next = private->roots;
	private->roots = root;
}


static void remove_root(struct lis_bw_impl_private *private, struct lis_bw_item *root)
{
	struct lis_bw_item *ptr, *pptr;

	for (pptr = NULL, ptr = private->roots ;
		ptr != NULL ;
		pptr = ptr, ptr = ptr->next) {

		if (ptr == root || strcasecmp(ptr->parent.name, root->parent.name) == 0) {
			if (pptr == NULL) {
				private->roots = ptr->next;
			} else {
				pptr->next = ptr->next;
			}
			return;
		}
	}

	lis_log_warning("Tried to remove unknown root item: %s", root->parent.name);
}


static enum lis_error lis_bw_get_device(struct lis_api *impl, const char *dev_id, struct lis_item **item)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	struct lis_bw_item *out = calloc(1, sizeof(struct lis_bw_item));
	enum lis_error err;

	if (out == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	err = private->wrapped->get_device(private->wrapped, dev_id, &out->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("%s: get_device() failed: %d, %s",
			private->wrapper_name, err, lis_strerror(err));
		return err;
	}
	memcpy(&out->parent, &g_bw_item_root_template, sizeof(out->parent));
	out->parent.name = out->wrapped->name;
	out->parent.type = out->wrapped->type;
	out->root = out;
	out->impl = private;

	if (private->item_filter.cb == NULL) {
		lis_log_info("%s: No item filter defined. Returning root item as is.",
			private->wrapper_name);
	} else {
		err = private->item_filter.cb(
			&out->parent, 1 /* root */, private->item_filter.user_data
		);
		if (LIS_IS_ERROR(err)) {
			out->wrapped->close(out->wrapped);
			FREE(out);
			return err;
		}
	}

	add_root(private, out);

	*item = &out->parent;
	return LIS_OK;
}


enum lis_error lis_api_base_wrapper(struct lis_api *to_wrap, struct lis_api **out, const char *wrapper_name)
{
	struct lis_bw_impl_private *private = calloc(1, sizeof(struct lis_bw_impl_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(&private->parent, &g_bw_impl_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;
	private->wrapped = to_wrap;
	private->wrapper_name = wrapper_name;

	private->next = g_impls;
	g_impls = private;

	*out = &private->parent;
	return LIS_OK;
}


void lis_bw_set_opt_desc_filter(struct lis_api *impl, lis_bw_opt_desc_filter filter, void *user_data)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	private->opt_desc_filter.cb = filter;
	private->opt_desc_filter.user_data = user_data;
}


static enum lis_error dup_opt_constraint(struct lis_option_descriptor *desc)
{
	union lis_value *dup;

	switch(desc->constraint.type)
	{
		case LIS_CONSTRAINT_NONE:
		case LIS_CONSTRAINT_RANGE:
			return LIS_OK;
		case LIS_CONSTRAINT_LIST:
			dup = calloc(desc->constraint.possible.list.nb_values, sizeof(union lis_value));
			if (dup == NULL) {
				return LIS_ERR_NO_MEM;
			}
			memcpy(dup, desc->constraint.possible.list.values,
					desc->constraint.possible.list.nb_values * sizeof(union lis_value));
			desc->constraint.possible.list.values = dup;
			return LIS_OK;
	}
	lis_log_error("Unknown constraint type: %s : %d", desc->name, desc->constraint.type);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}


static void free_opt_constraint(struct lis_option_descriptor *desc)
{
	switch(desc->constraint.type)
	{
		case LIS_CONSTRAINT_NONE:
		case LIS_CONSTRAINT_RANGE:
			return;
		case LIS_CONSTRAINT_LIST:
			FREE(desc->constraint.possible.list.values);
			return;
	}
	lis_log_error("Unknown constraint type: %s : %d", desc->name, desc->constraint.type);
}

static void free_opt_user(struct lis_bw_option_descriptor *opt)
{
	if (opt->user != NULL && opt->free_cb != NULL) {
		opt->free_cb(opt->user);
		opt->user = NULL;
	}
}

static void free_options(struct lis_bw_item *item)
{
	int i;
	if (item->children != NULL) {
		for (i = 0 ; item->children[i] != NULL ; i++) {
			free_options(item->children[i]);
		}
	}
	if (item->options != NULL) {
		for (i = 0 ; item->options[i] != NULL ; i++) {
			free_opt_constraint(&item->options[i]->basewrapper);
			free_opt_user(item->options[i]);
		}
		FREE(item->options[0]);
	}
	FREE(item->options);
}


static void free_children(struct lis_bw_item *item)
{
	int i;
	if (item->children != NULL) {
		for (i = 0 ; item->children[i] != NULL ; i++) {
			free_children(item->children[i]);
		}
		FREE(item->children[0]);
	}
	FREE(item->children);
}


static enum lis_error lis_bw_item_get_children(struct lis_item *self, struct lis_item ***out_children)
{
	struct lis_bw_item *private = LIS_BW_ITEM(self);
	struct lis_item **to_wrap;
	struct lis_bw_item *items;
	int nb_items, i;
	enum lis_error err;

	if (private->children != NULL) {
		*out_children = ((struct lis_item **)private->children);
		return LIS_OK;
	}

	err = private->wrapped->get_children(private->wrapped, &to_wrap);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("%s: get_children() failed: %d, %s",
			private->impl->wrapper_name, err, lis_strerror(err));
		return err;
	}
	for (nb_items = 0 ; to_wrap[nb_items] != NULL ; nb_items++) { }

	private->children = calloc(nb_items + 1, sizeof(struct lis_bw_item *));
	if (private->children == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	if (nb_items > 0) {
		items = calloc(nb_items, sizeof(struct lis_bw_item));
		if (items == NULL) {
			FREE(items);
			lis_log_error("Out of memory");
			return LIS_ERR_NO_MEM;
		}
		for (i = 0 ; i < nb_items ; i++) {
			private->children[i] = &items[i];
			memcpy(&private->children[i]->parent, &g_bw_item_child_template,
					sizeof(private->children[i]->parent));
			private->children[i]->wrapped = to_wrap[i];
			private->children[i]->root = private;
			private->children[i]->parent.name = to_wrap[i]->name;
			private->children[i]->parent.type = to_wrap[i]->type;
			private->children[i]->impl = private->impl;

			if (private->impl->item_filter.cb != NULL) {
				err = private->impl->item_filter.cb(
					&private->children[i]->parent, 0 /* !root */,
					private->impl->item_filter.user_data
				);
				if (LIS_IS_ERROR(err)) {
					FREE(items);
					return err;
				}
			}
		}
	}

	*out_children = ((struct lis_item **)private->children);
	return LIS_OK;
}




static enum lis_error lis_bw_item_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
)
{
	struct lis_bw_item *private = LIS_BW_ITEM(self);
	struct lis_option_descriptor **opts;
	int nb_opts, i;
	enum lis_error err;

	err = private->wrapped->get_options(private->wrapped, &opts);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	if(private->impl->opt_desc_filter.cb == NULL) {
		lis_log_debug("%s: No option filter defined. Returning options as is.",
			private->impl->wrapper_name);
		*descs = opts;
		return err;
	}

	/* free any previous options */
	if (private->options != NULL) {
		for (i = 0 ; private->options[i] != NULL ; i++) {
			free_opt_constraint(&private->options[i]->basewrapper);
			free_opt_user(private->options[i]);
		}
		FREE(private->options[0]);
	}

	for (nb_opts = 0 ; opts[nb_opts] != NULL ; nb_opts++) { }
	private->options = calloc(nb_opts + 1, sizeof(struct lis_bw_option_descriptor *));
	if (nb_opts > 0) {
		/* duplicate the options so the filter can modify them */
		private->options[0] = calloc(nb_opts, sizeof(struct lis_bw_option_descriptor));
		for (i = 0 ; i < nb_opts ; i++) {
			private->options[i] = private->options[0] + i;
			memcpy(&private->options[i]->parent, opts[i], sizeof(private->options[i]->parent));
			private->options[i]->parent.fn.get_value = lis_bw_get_value;
			private->options[i]->parent.fn.set_value = lis_bw_set_value;
			private->options[i]->item = private;
			private->options[i]->wrapped = opts[i];
			err = dup_opt_constraint(&private->options[i]->parent);
			if (LIS_IS_ERROR(err)) {
				FREE(private->options[0]);
				FREE(private->options);
				return err;
			}
			memcpy(
				&private->options[i]->basewrapper,
				&private->options[i]->parent,
				sizeof(private->options[i]->basewrapper)
			);
		}
		/* and filter */
		for (i = 0 ; i < nb_opts ; i++) {
			err = private->impl->opt_desc_filter.cb(
				self, &private->options[i]->parent,
				private->impl->opt_desc_filter.user_data
			);
			if (LIS_IS_ERROR(err)) {
				lis_log_warning("%s: option filter returned an error: %d, %s",
						private->impl->wrapper_name, err, lis_strerror(err));
				FREE(private->options[0]);
				FREE(private->options);
				return err;
			}
		}
	}
	*descs = (struct lis_option_descriptor **)private->options;
	return LIS_OK;
}


static enum lis_error lis_bw_item_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	)
{
	struct lis_bw_item *item = LIS_BW_ITEM(self);

	if (item->impl->scan_start.cb != NULL) {
		return item->impl->scan_start.cb(
			self, session, item->impl->scan_start.user_data
		);
	}

	return item->wrapped->scan_start(item->wrapped, session);
}


static void lis_bw_item_root_close(struct lis_item *self)
{
	struct lis_bw_item *item = LIS_BW_ITEM(self);
	int i;

	if (item->impl->on_close_item.cb != NULL) {
		if (item->children != NULL) {
			for (i = 0 ; item->children[i] != NULL ; i++) {
				item->impl->on_close_item.cb(
					&item->children[i]->parent, 1 /* children */,
					item->impl->on_close_item.user_data
				);
			}
		}
		item->impl->on_close_item.cb(
			self, 0 /* root */,
			item->impl->on_close_item.user_data
		);
	}

	remove_root(item->impl, item);
	item->wrapped->close(item->wrapped);
	free_options(item);
	free_children(item);
	FREE(item);
}


static void lis_bw_item_child_close(struct lis_item *self)
{
	struct lis_bw_item *item = LIS_BW_ITEM(self);
	item->wrapped->close(item->wrapped);
}


struct lis_item *lis_bw_get_original_item(struct lis_item *modified)
{
	struct lis_bw_item *item = LIS_BW_ITEM(modified);
	return item->wrapped;
}


struct lis_item *lis_bw_get_root_item(struct lis_item *child)
{
	struct lis_bw_item *private = LIS_BW_ITEM(child);
	return &private->root->parent;
}


struct lis_option_descriptor *lis_bw_get_original_opt(struct lis_option_descriptor *modified)
{
	struct lis_bw_option_descriptor *private = LIS_BW_OPT_DESC(modified);
	return private->wrapped;
}


static enum lis_error lis_bw_get_value(struct lis_option_descriptor *self, union lis_value *value)
{
	struct lis_bw_option_descriptor *private = LIS_BW_OPT_DESC(self);
	return private->wrapped->fn.get_value(private->wrapped, value);
}


static enum lis_error lis_bw_set_value(
		struct lis_option_descriptor *self, union lis_value value, int *set_flags
	)
{
	struct lis_bw_option_descriptor *private = LIS_BW_OPT_DESC(self);
	return private->wrapped->fn.set_value(private->wrapped, value, set_flags);
}


void lis_bw_set_item_filter(struct lis_api *impl, lis_bw_item_filter filter, void *user_data)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	private->item_filter.cb = filter;
	private->item_filter.user_data = user_data;
}


void lis_bw_item_set_user_ptr(struct lis_item *self, void *user_ptr)
{
	struct lis_bw_item *item = LIS_BW_ITEM(self);
	item->user = user_ptr;
}


void *lis_bw_item_get_user_ptr(struct lis_item *self)
{
	struct lis_bw_item *item = LIS_BW_ITEM(self);
	return item->user;
}


void lis_bw_opt_set_user_ptr(struct lis_option_descriptor *opt, void *user_ptr, lis_bw_free_fn free_cb)
{
	struct lis_bw_option_descriptor *private = LIS_BW_OPT_DESC(opt);
	private->user = user_ptr;
	private->free_cb = free_cb;
}


void *lis_bw_opt_get_user_ptr(struct lis_option_descriptor *opt)
{
	struct lis_bw_option_descriptor *private = LIS_BW_OPT_DESC(opt);
	return private->user;
}


void lis_bw_set_on_scan_start(
		struct lis_api *impl, lis_bw_on_scan_start cb, void *user_data
	)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	private->scan_start.cb = cb;
	private->scan_start.user_data = user_data;
}


void lis_bw_set_on_close_item(struct lis_api *impl, lis_bw_on_close_item cb, void *user_data)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	private->on_close_item.cb = cb;
	private->on_close_item.user_data = user_data;
}


void lis_bw_set_clean_impl(struct lis_api *impl, lis_bw_clean_impl cb, void *user_data)
{
	struct lis_bw_impl_private *private = LIS_BW_IMPL_PRIVATE(impl);
	private->impl_clean.cb = cb;
	private->impl_clean.user_data = user_data;
}
