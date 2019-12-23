#include <assert.h>
#include <string.h>

#include <libinsane/constants.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "../basewrapper.h"


#define NAME "lamp"


static enum lis_error lis_lamp_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *params
);
static int lis_lamp_end_of_feed(struct lis_scan_session *session);
static int lis_lamp_end_of_page(struct lis_scan_session *session);
static enum lis_error lis_lamp_scan_read(
	struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
);
static void lis_lamp_cancel(struct lis_scan_session *session);


struct lis_lamp_scan_session
{
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_item *root;
	struct lis_item *item;
};
#define LIS_LAMP_SCAN_SESSION_PRIVATE(session) \
	((struct lis_lamp_scan_session *)(session))


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = lis_lamp_get_scan_parameters,
	.end_of_feed = lis_lamp_end_of_feed,
	.end_of_page = lis_lamp_end_of_page,
	.scan_read = lis_lamp_scan_read,
	.cancel = lis_lamp_cancel,
};


static void set_lamp_switch(struct lis_item *item, int lamp_switch)
{
	enum lis_error err;
	struct lis_option_descriptor **opts;
	int opt_idx;
	union lis_value val = { .boolean = lamp_switch };
	int set_flags;

	lis_log_info("Setting lamp on %s to %d", item->name, lamp_switch);

	err = item->get_options(item, &opts);
	if (LIS_IS_ERROR(err)) {
		lis_log_warning(
			"Cannot set lamp on %s to %d: Failed to get options: %d, %s",
			item->name, lamp_switch, err, lis_strerror(err)
		);
		return;
	}

	for (opt_idx = 0 ; opts[opt_idx] != NULL ; opt_idx++) {
		if (strcasecmp(opts[opt_idx]->name, OPT_NAME_LAMP_SWITCH) == 0) {
			err = opts[opt_idx]->fn.set_value(opts[opt_idx], val, &set_flags);
			if (LIS_IS_ERROR(err)) {
				lis_log_warning(
					"Cannot set lamp on %s to %d:"
					" Failed to set option: %d, %s",
					item->name, lamp_switch,
					err, lis_strerror(err)
				);
				return;
			}
			if (set_flags != 0) {
				lis_log_warning(
					"When set lamp on %s to %d, a set_flag"
					" was returned: 0x%X",
					item->name, lamp_switch, set_flags
				);
				return;
			}
			return;
		}
	}
}


static enum lis_error lamp_scan_start(
		struct lis_item *item, struct lis_scan_session **out,
		void *user_data
	)
{
	struct lis_item *original = lis_bw_get_original_item(item);
	struct lis_item *root = lis_bw_get_root_item(item);
	struct lis_lamp_scan_session *private;
	enum lis_error err;

	LIS_UNUSED(user_data);

	private = lis_bw_item_get_user_ptr(root);
	if (private != NULL) {
		FREE(private);
		lis_bw_item_set_user_ptr(root, NULL);
	}

	private = calloc(1, sizeof(struct lis_lamp_scan_session));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	set_lamp_switch(original, 1);

	err = original->scan_start(original, &private->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"scan_start() failed: 0x%X, %s",
			err, lis_strerror(err)
		);
		FREE(private);
		return err;
	}
	memcpy(&private->parent, &g_scan_session_template, sizeof(private->parent));
	private->root = root;
	private->item = item;

	lis_bw_item_set_user_ptr(root, private);

	*out = &private->parent;
	return err;
}


static enum lis_error lis_lamp_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	struct lis_lamp_scan_session *private = LIS_LAMP_SCAN_SESSION_PRIVATE(self);
	return private->wrapped->get_scan_parameters(private->wrapped, params);
}



static int lis_lamp_end_of_feed(struct lis_scan_session *session)
{
	struct lis_lamp_scan_session *private = LIS_LAMP_SCAN_SESSION_PRIVATE(session);
	int r;

	r = private->wrapped->end_of_feed(private->wrapped);
	if (r) {
		set_lamp_switch(private->item, 0);
	}
	return r;
}


static int lis_lamp_end_of_page(struct lis_scan_session *session)
{
	struct lis_lamp_scan_session *private = LIS_LAMP_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->end_of_page(private->wrapped);
}


static enum lis_error lis_lamp_scan_read(
		struct lis_scan_session *session,
		void *out_buffer, size_t *buffer_size
	)
{
	struct lis_lamp_scan_session *private = LIS_LAMP_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->scan_read(private->wrapped, out_buffer, buffer_size);
}


static void lis_lamp_cancel(struct lis_scan_session *session)
{
	struct lis_lamp_scan_session *private = LIS_LAMP_SCAN_SESSION_PRIVATE(session);
	private->wrapped->cancel(private->wrapped);
	set_lamp_switch(private->item, 0);
	lis_bw_item_set_user_ptr(private->root, NULL);
	FREE(private);
}


static void lamp_on_item_close(struct lis_item *item, int root, void *user_data)
{
	struct lis_lamp_scan_session *private;

	LIS_UNUSED(user_data);

	if (!root) {
		return;
	}

	lis_log_debug("Closing %s", item->name);
	private = lis_bw_item_get_user_ptr(item); // root
	if (private == NULL) {
		return;
	}

	lis_lamp_cancel(&private->parent);
	lis_log_debug("%s closed", item->name);
}


enum lis_error lis_api_workaround_lamp(
		struct lis_api *to_wrap, struct lis_api **out_impl
	)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, out_impl, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	lis_bw_set_on_close_item(*out_impl, lamp_on_item_close, NULL);
	lis_bw_set_on_scan_start(*out_impl, lamp_scan_start, NULL);

	return err;
}
