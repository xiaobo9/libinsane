#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>


struct alias
{
	const char *opt_name;
	const char **alias_for;
	enum {
		ALIAS_REQ_ANY_OPTIONS = 0,
		ALIAS_REQ_ALL_OPTIONS,
	} requires;

	enum lis_error (*get_value)(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value *value
	);
	enum lis_error (*set_value)(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value value,
		int *set_flags
	);
};


struct aliases {
	struct lis_api parent;
	struct lis_api *wrapped;
};
#define ALIASES_PRIVATE(impl) ((struct aliases *)(impl))


struct aliases_item {
	struct lis_item parent;
	struct lis_item *wrapped;

	struct aliases_item *children;
	struct lis_item **children_ptr;

	struct aliases_opt *opts; /* only aliases */
	struct lis_option_descriptor **opts_ptr; /* all options, aliases included */
};
#define ALIASES_ITEM_PRIVATE(item) ((struct aliases_item *)(item))


struct aliases_opt {
	struct lis_option_descriptor parent;

	struct aliases_item *item;
	const struct alias *alias;
	struct lis_option_descriptor **opts; // underlying options ; no aliases
};
#define ALIASES_OPT_PRIVATE(opt) ((struct aliases_opt *)(opt))


static enum lis_error simple_alias_get_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value *value
);
static enum lis_error simple_alias_set_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value value,
	int *set_flags
);


static enum lis_error tl_get_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value *value
);
static enum lis_error tl_set_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value value,
	int *set_flags
);


static enum lis_error br_get_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value *value
);
static enum lis_error br_set_value(
	struct lis_option_descriptor *opt,
	const struct alias *alias,
	struct lis_option_descriptor **opts,
	union lis_value value,
	int *set_flags
);


static const struct alias g_aliases[] = {
	{
		.opt_name = OPT_NAME_RESOLUTION,
		.requires = ALIAS_REQ_ANY_OPTIONS,
		.alias_for = (const char *[]) { "xres", "yres", NULL },
		.get_value = simple_alias_get_value,
		.set_value = simple_alias_set_value,
	},
	{
		.opt_name = OPT_NAME_TL_X,
		.requires = ALIAS_REQ_ALL_OPTIONS,
		.alias_for = (const char *[]) { "xpos", "xextent", NULL },
		.get_value = tl_get_value,
		.set_value = tl_set_value,
	},
	{
		.opt_name = OPT_NAME_TL_Y,
		.requires = ALIAS_REQ_ALL_OPTIONS,
		.alias_for = (const char *[]) { "ypos", "yextent", NULL },
		.get_value = tl_get_value,
		.set_value = tl_set_value,
	},
	{
		.opt_name = OPT_NAME_BR_X,
		.requires = ALIAS_REQ_ALL_OPTIONS,
		.alias_for = (const char *[]) { "xpos", "xextent", NULL },
		.get_value = br_get_value,
		.set_value = br_set_value,
	},
	{
		.opt_name = OPT_NAME_BR_Y,
		.requires = ALIAS_REQ_ALL_OPTIONS,
		.alias_for = (const char *[]) { "ypos", "yextent", NULL },
		.get_value = br_get_value,
		.set_value = br_set_value,
	},
};


static void aliases_cleanup(struct lis_api *impl);
static enum lis_error aliases_list_devices(
	struct lis_api *impl, enum lis_device_locations locs, struct lis_device_descriptor ***dev_infos
);
static enum lis_error aliases_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item
);


static const struct lis_api g_aliases_template = {
	.cleanup = aliases_cleanup,
	.list_devices = aliases_list_devices,
	.get_device = aliases_get_device,
};


static enum lis_error aliases_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error aliases_get_options(
	struct lis_item *self, struct lis_option_descriptor ***descs
);
static enum lis_error aliases_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void aliases_close(struct lis_item *self);


static const struct lis_item g_item_template = {
	.get_children = aliases_get_children,
	.get_options = aliases_get_options,
	.scan_start = aliases_scan_start,
	.close = aliases_close,
};


enum lis_error lis_api_normalizer_opt_aliases(struct lis_api *to_wrap, struct lis_api **api)
{
	struct aliases *private;

	private = calloc(1, sizeof(struct aliases));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	memcpy(&private->parent, &g_aliases_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;
	private->wrapped = to_wrap;

	*api = &private->parent;
	return LIS_OK;
}


static void aliases_cleanup(struct lis_api *impl)
{
	struct aliases *private = ALIASES_PRIVATE(impl);
	private->wrapped->cleanup(private->wrapped);
	FREE(private);
}


static enum lis_error aliases_list_devices(
		struct lis_api *impl, enum lis_device_locations locs, struct lis_device_descriptor ***dev_infos
	)
{
	struct aliases *private = ALIASES_PRIVATE(impl);
	return private->wrapped->list_devices(private->wrapped, locs, dev_infos);
}


static enum lis_error aliases_get_device(
		struct lis_api *impl, const char *dev_id, struct lis_item **item
	)
{
	struct aliases *private_impl = ALIASES_PRIVATE(impl);
	struct aliases_item *private_item;
	enum lis_error err;

	private_item = calloc(1, sizeof(struct aliases_item));
	if (private_item == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	err = private_impl->wrapped->get_device(private_impl->wrapped, dev_id, &private_item->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("get_device() failed: 0x%X, %s", err, lis_strerror(err));
		FREE(private_item);
		return err;
	}

	memcpy(&private_item->parent, &g_item_template, sizeof(private_item->parent));
	private_item->parent.name = private_item->wrapped->name;
	private_item->parent.type = private_item->wrapped->type;

	*item = &private_item->parent;
	return err;
}


static void free_children(struct aliases_item *private)
{
	int i;
	if (private->children != NULL) {
		for (i = 0 ; private->children_ptr[i] != NULL ; i++) {
			FREE(private->children[i].opts);
			FREE(private->children[i].opts_ptr);
			free_children(&private->children[i]);
		}
	}
	FREE(private->children);
	FREE(private->children_ptr);
}


static enum lis_error aliases_get_children(struct lis_item *self, struct lis_item ***out_children)
{
	struct aliases_item *private = ALIASES_ITEM_PRIVATE(self);
	struct lis_item **children;
	enum lis_error err;
	int nb_children, i;

	err = private->wrapped->get_children(private->wrapped, &children);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("get_children() failed: 0x%X, %s", err, lis_strerror(err));
		return err;
	}

	free_children(private);

	for (nb_children = 0 ; children[nb_children] != NULL ; nb_children++) { }

	if (nb_children <= 0) {
		*out_children = children;
		return LIS_OK;
	}

	private->children = calloc(nb_children, sizeof(struct aliases_item));
	private->children_ptr = calloc(nb_children + 1, sizeof(struct lis_item *));
	if (private->children == NULL || private->children_ptr == NULL) {
		FREE(private->children);
		FREE(private->children_ptr);
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (i = 0 ; i < nb_children ; i++) {
		private->children_ptr[i] = &private->children[i].parent;
		memcpy(&private->children[i].parent, &g_item_template,
				sizeof(private->children[i].parent));
		private->children[i].parent.name = children[i]->name;
		private->children[i].parent.type = children[i]->type;
		private->children[i].wrapped = children[i];
	}

	*out_children = private->children_ptr;
	return err;
}


static struct lis_option_descriptor *get_option(struct lis_option_descriptor **descs, const char *name)
{
	int i;
	for (i = 0 ; descs[i] != NULL ; i++) {
		if (strcasecmp(descs[i]->name, name) == 0) {
			return descs[i];
		}
	}
	return NULL;
}


static enum lis_error opt_get_value(struct lis_option_descriptor *self, union lis_value *value)
{
	struct aliases_opt *private = ALIASES_OPT_PRIVATE(self);
	return private->alias->get_value(
		self, private->alias,
		private->opts, value);
}


static enum lis_error opt_set_value(struct lis_option_descriptor *self, union lis_value value, int *set_flags)
{
	struct aliases_opt *private = ALIASES_OPT_PRIVATE(self);
	return private->alias->set_value(
		self, private->alias,
		private->opts, value,
		set_flags
	);
}


static enum lis_error aliases_get_options(
		struct lis_item *self, struct lis_option_descriptor ***out_descs
	)
{
	struct aliases_item *private = ALIASES_ITEM_PRIVATE(self);
	enum lis_error err;
	struct lis_option_descriptor **descs;
	struct lis_option_descriptor *opt;
	int nb_opts;
	int aliased_idx;
	unsigned int alias_idx;
	int caps;
	int got_one, got_all;

	err = private->wrapped->get_options(private->wrapped, &descs);
	if (!LIS_IS_OK(err)) {
		return err;
	}

	for (nb_opts = 0 ; descs[nb_opts] != NULL ; nb_opts++) { }

	FREE(private->opts);
	FREE(private->opts_ptr);

	private->opts = calloc(LIS_COUNT_OF(g_aliases), sizeof(struct aliases_opt));
	private->opts_ptr = calloc(
		nb_opts + LIS_COUNT_OF(g_aliases) + 1,
		sizeof(struct lis_option_descriptor *)
	);
	if (private->opts == NULL || private->opts_ptr == NULL) {
		FREE(private->children);
		FREE(private->opts_ptr);
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (nb_opts = 0 ; descs[nb_opts] != NULL ; nb_opts++) {
		private->opts_ptr[nb_opts] = descs[nb_opts];
	}

	for (alias_idx = 0 ; alias_idx < LIS_COUNT_OF(g_aliases) ; alias_idx++) {
		got_one = 0;
		got_all = 1;
		caps = 0;
		for (aliased_idx = 0 ; g_aliases[alias_idx].alias_for[aliased_idx] != NULL ; aliased_idx++) {
			opt = get_option(descs, g_aliases[alias_idx].alias_for[aliased_idx]);
			if (opt == NULL) {
				got_all = 0;
			} else {
				got_one = 1;
				caps |= opt->capabilities;
				// ASSUMPTION(Jflesch): All options are compatible
				memcpy(
					&private->opts[alias_idx].parent, opt,
					sizeof(private->opts[alias_idx].parent)
				);
			}
		}

		if (!got_one) {
			got_all = 0;
		}

		caps |= LIS_CAP_EMULATED;
		private->opts[alias_idx].item = private;
		private->opts[alias_idx].opts = descs;
		private->opts[alias_idx].alias = &g_aliases[alias_idx];
		private->opts[alias_idx].parent.name = g_aliases[alias_idx].opt_name;
		private->opts[alias_idx].parent.capabilities = caps;
		private->opts[alias_idx].parent.fn.set_value = opt_set_value;
		private->opts[alias_idx].parent.fn.get_value = opt_get_value;

		switch(g_aliases[alias_idx].requires) {
			case ALIAS_REQ_ANY_OPTIONS:
				if (!got_one) {
					continue;
				}
				break;
			case ALIAS_REQ_ALL_OPTIONS:
				if (!got_all) {
					continue;
				}
				break;
		}

		lis_log_info("Adding alias option '%s'", g_aliases[alias_idx].opt_name);
		private->opts_ptr[nb_opts] = &private->opts[alias_idx].parent;
		nb_opts++;
	}

	*out_descs = private->opts_ptr;
	return LIS_OK;
}


static enum lis_error aliases_scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	struct aliases_item *private = ALIASES_ITEM_PRIVATE(self);
	return private->wrapped->scan_start(private->wrapped, session);
}


static void aliases_close(struct lis_item *self)
{
	struct aliases_item *private = ALIASES_ITEM_PRIVATE(self);
	free_children(private);
	FREE(private->opts);
	FREE(private->opts_ptr);
	FREE(private);
}


static enum lis_error simple_alias_get_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value *value
	)
{
	int aliased_idx;
	struct lis_option_descriptor *aliased;

	LIS_UNUSED(opt);

	for (aliased_idx = 0 ; alias->alias_for[aliased_idx] != NULL ; aliased_idx++) {
		aliased = get_option(opts, alias->alias_for[aliased_idx]);
		if (aliased != NULL) {
			return aliased->fn.get_value(aliased, value);
		}
	}
	assert(0);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}


static enum lis_error simple_alias_set_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value value,
		int *out_set_flags
	)
{
	enum lis_error err, out_err = LIS_OK;
	int aliased_idx;
	struct lis_option_descriptor *aliased;
	int set_flags;

	for (aliased_idx = 0 ; alias->alias_for[aliased_idx] != NULL ; aliased_idx++) {
		aliased = get_option(opts, alias->alias_for[aliased_idx]);
		if (aliased != NULL) {
			err = aliased->fn.set_value(aliased, value, &set_flags);
			if (LIS_IS_OK(err)) {
				*out_set_flags |= set_flags;
			} else {
				lis_log_error(
					"set_value(%s): Failed to set"
					" value of '%s': 0x%X, %s",
					opt->name, aliased->name,
					err, lis_strerror(err)
				);
				out_err = err;
			}
		}
	}
	return out_err;
}


static enum lis_error tl_get_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value *value
	)
{
	struct lis_option_descriptor *aliased;

	LIS_UNUSED(opt);

	aliased = get_option(opts, alias->alias_for[0]);
	assert(aliased != NULL);
	return aliased->fn.get_value(aliased, value);
}


static enum lis_error tl_set_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value in_value,
		int *out_set_flags
	)
{
	struct lis_option_descriptor *opt_pos;
	struct lis_option_descriptor *opt_extent;
	enum lis_error err;
	union lis_value val_pos;
	union lis_value val_extent;
	union lis_value val_total;
	int set_flags;

	LIS_UNUSED(opt);

	opt_pos = get_option(opts, alias->alias_for[0]);
	assert(opt_pos != NULL);
	opt_extent = get_option(opts, alias->alias_for[1]);
	assert(opt_extent != NULL);

	err = opt_pos->fn.get_value(opt_pos, &val_pos);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get value of '%s': 0x%X, %s",
			opt_pos->name, err, lis_strerror(err)
		);
		return err;
	}
	err = opt_extent->fn.get_value(opt_extent, &val_extent);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get value of '%s': 0x%X, %s",
			opt_extent->name, err, lis_strerror(err)
		);
		return err;
	}

	val_total = lis_add(opt_pos->value.type, val_pos, val_extent);
	val_extent = lis_sub(opt_pos->value.type, val_total, in_value);

	*out_set_flags = 0;

	err = opt_pos->fn.set_value(opt_pos, in_value, &set_flags);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to set value of '%s': 0x%X, %s",
			opt_pos->name, err, lis_strerror(err)
		);
		return err;
	}
	*out_set_flags |= set_flags;

	err = opt_extent->fn.set_value(opt_extent, val_extent, &set_flags);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to set value of '%s': 0x%X, %s",
			opt_extent->name, err, lis_strerror(err)
		);
		return err;
	}
	*out_set_flags |= set_flags;

	return LIS_OK;
}


static enum lis_error br_get_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value *value
	)
{
	struct lis_option_descriptor *opt_pos;
	struct lis_option_descriptor *opt_extent;
	union lis_value val_pos;
	union lis_value val_extent;
	enum lis_error err;

	LIS_UNUSED(opt);

	opt_pos = get_option(opts, alias->alias_for[0]);
	assert(opt_pos != NULL);
	opt_extent = get_option(opts, alias->alias_for[1]);
	assert(opt_extent != NULL);

	err = opt_pos->fn.get_value(opt_pos, &val_pos);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get value of '%s': 0x%X, %s",
			opt_pos->name, err, lis_strerror(err)
		);
		return err;
	}

	err = opt_extent->fn.get_value(opt_extent, &val_extent);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get value of '%s': 0x%X, %s",
			opt_extent->name, err, lis_strerror(err)
		);
		return err;
	}

	*value = lis_add(opt_pos->value.type, val_pos, val_extent);
	return LIS_OK;
}


static enum lis_error br_set_value(
		struct lis_option_descriptor *opt,
		const struct alias *alias,
		struct lis_option_descriptor **opts,
		union lis_value in_value,
		int *out_set_flags
	)
{
	struct lis_option_descriptor *opt_pos;
	struct lis_option_descriptor *opt_extent;
	enum lis_error err;
	union lis_value val_pos;
	union lis_value val_extent;

	LIS_UNUSED(opt);

	opt_pos = get_option(opts, alias->alias_for[0]);
	assert(opt_pos != NULL);
	opt_extent = get_option(opts, alias->alias_for[1]);
	assert(opt_extent != NULL);

	err = opt_pos->fn.get_value(opt_pos, &val_pos);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to get value of '%s': 0x%X, %s",
			opt_pos->name, err, lis_strerror(err)
		);
		return err;
	}

	val_extent = lis_sub(opt_pos->value.type, in_value, val_pos);

	err = opt_pos->fn.set_value(opt_extent, val_extent, out_set_flags);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"Failed to set value of '%s': 0x%X, %s",
			opt_extent->name, err, lis_strerror(err)
		);
	}
	return err;
}
