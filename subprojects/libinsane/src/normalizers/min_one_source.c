#include <assert.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>


#include "../basewrapper.h"

#define NAME "normalizers_min_one_source"


struct mos_child
{
	struct lis_item parent;
	struct lis_item *wrapped;

	void (*previous_close_cb)(struct lis_item *item);
	struct lis_item *ptrs[2];
};
#define LIS_MOS_CHILD(item) ((struct mos_child *)(item))


static enum lis_error child_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error item_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	);
static enum lis_error item_get_scan_parameters(
		struct lis_item *self, struct lis_scan_parameters *parameters
	);
static enum lis_error item_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void child_close(struct lis_item *self);


static struct lis_item g_mos_item_template =
{
	.get_children = child_get_children,
	.get_options = item_get_options,
	.get_scan_parameters = item_get_scan_parameters,
	.scan_start = item_scan_start,
	.close = child_close,
};


static void root_close(struct lis_item *self)
{
	struct mos_child *child;
	child = lis_bw_item_get_user_ptr(self);
	child->previous_close_cb(self);
	free(child);
}


static enum lis_error root_get_children(struct lis_item *self, struct lis_item ***children)
{
	enum lis_error err;
	struct lis_item *original;
	struct mos_child *child;

	original = lis_bw_get_original_item(self);
	err = original->get_children(original, children);
	if (LIS_IS_ERROR(err) || (*children)[0] != NULL) {
		return err;
	}

	lis_log_info(NAME ": root->children() return no children. Faking one children.");

	child = lis_bw_item_get_user_ptr(self);
	free(child);

	child = calloc(1, sizeof(struct mos_child));
	if (child == NULL) {
		lis_log_error(NAME ": Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(&child->parent, &g_mos_item_template, sizeof(child->parent));
	child->parent.name = OPT_VALUE_SOURCE_ADF;
	child->parent.type = original->type;
	if (child->parent.type == LIS_ITEM_UNIDENTIFIED) {
		child->parent.type = LIS_ITEM_ADF;
	}
	child->ptrs[0] = &child->parent;
	child->ptrs[1] = NULL;
	child->wrapped = original;
	*children = child->ptrs;

	child->previous_close_cb = self->close;
	self->close = root_close;

	lis_bw_item_set_user_ptr(self, child);
	return LIS_OK;
}


static enum lis_error item_filter(struct lis_item *item, int root, void *user_data)
{
	LIS_UNUSED(user_data);
	if (root) {
		item->get_children = root_get_children;
	}
	return LIS_OK;
}


enum lis_error lis_api_normalizer_min_one_source(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;
	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_OK(err)) {
		lis_bw_set_item_filter(*impl, item_filter, NULL);
	}
	return err;
}


static enum lis_error child_get_children(struct lis_item *self, struct lis_item ***children)
{
	struct mos_child *private = LIS_MOS_CHILD(self);
	enum lis_error err;
	err = private->wrapped->get_children(private->wrapped, children);
	assert((*children)[0] == NULL);
	return err;
}


static enum lis_error item_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	struct mos_child *private = LIS_MOS_CHILD(self);
	return private->wrapped->get_options(private->wrapped, descs);
}


static enum lis_error item_get_scan_parameters(
		struct lis_item *self, struct lis_scan_parameters *parameters
	)
{
	struct mos_child *private = LIS_MOS_CHILD(self);
	return private->wrapped->get_scan_parameters(private->wrapped, parameters);
}


static enum lis_error item_scan_start(struct lis_item *self, struct lis_scan_session **session)
{
	struct mos_child *private = LIS_MOS_CHILD(self);
	return private->wrapped->scan_start(private->wrapped, session);
}


static void child_close(struct lis_item *self)
{
	LIS_UNUSED(self);
	/* do nothing, should only be done on the root */
}
