#include <stdlib.h>
#include <string.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "../basewrapper.h"

#define NAME "one_page_flatbed"


struct one_scan_session_private {
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_item *bw_item; /* basewrapper item */
	struct lis_item *item; /* wrapped item (underlying implementation) */
};
#define ONE_SCAN_SESSION_PRIVATE(session) \
	((struct one_scan_session_private *)(session))


static enum lis_error one_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *params
);
static int one_end_of_feed(struct lis_scan_session *self);
static int one_end_of_page(struct lis_scan_session *self);
static enum lis_error one_scan_read(
	struct lis_scan_session *self,
	void *out_buffer, size_t *bufsize
);
static void one_cancel(struct lis_scan_session *self);


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = one_get_scan_parameters,
	.end_of_feed = one_end_of_feed,
	.end_of_page = one_end_of_page,
	.scan_read = one_scan_read,
	.cancel = one_cancel,
};


static enum lis_error one_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	struct one_scan_session_private *private = \
		ONE_SCAN_SESSION_PRIVATE(self);
	return private->wrapped->get_scan_parameters(
		private->wrapped,
		params
	);
}


static int one_end_of_feed(struct lis_scan_session *self)
{
	struct one_scan_session_private *private = \
		ONE_SCAN_SESSION_PRIVATE(self);
	switch(private->item->type) {
		case LIS_ITEM_FLATBED:
			// stop at the first page
			if (private->wrapped->end_of_page(private->wrapped)) {
				return 1;
			}
			break;
		case LIS_ITEM_ADF:
			break;
		case LIS_ITEM_DEVICE:
		case LIS_ITEM_UNIDENTIFIED:
			lis_log_warning(
				"Unexpected source type: %d (%s)."
				" Don't know whether we must stop at the"
				" first page."
				" Assuming the driver works as expected",
				private->item->type, private->item->name
			);
			break;
	}
	return private->wrapped->end_of_feed(private->wrapped);
}


static int one_end_of_page(struct lis_scan_session *self)
{
	struct one_scan_session_private *private = \
		ONE_SCAN_SESSION_PRIVATE(self);
	return private->wrapped->end_of_page(private->wrapped);
}


static enum lis_error one_scan_read(
		struct lis_scan_session *self,
		void *out_buffer, size_t *bufsize
	)
{
	struct one_scan_session_private *private = \
		ONE_SCAN_SESSION_PRIVATE(self);
	return private->wrapped->scan_read(
		private->wrapped, out_buffer, bufsize
	);
}


static void one_cancel(struct lis_scan_session *self)
{
	struct one_scan_session_private *private = \
		ONE_SCAN_SESSION_PRIVATE(self);
	lis_bw_item_set_user_ptr(private->bw_item, NULL);
	private->wrapped->cancel(private->wrapped);
	FREE(private);
}


static enum lis_error on_scan_start(
		struct lis_item *item, struct lis_scan_session **out_session,
		void *user_data
	)
{
	enum lis_error err;
	struct one_scan_session_private *session;

	LIS_UNUSED(user_data);

	session = lis_bw_item_get_user_ptr(item);
	FREE(session);
	lis_bw_item_set_user_ptr(item, NULL);

	session = calloc(1, sizeof(struct one_scan_session_private));
	if (session == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	session->bw_item = item;
	item = lis_bw_get_original_item(item);

	err = item->scan_start(item, &session->wrapped);
	if (LIS_IS_ERROR(err)) {
		FREE(session);
		return err;
	}

	memcpy(
		&session->parent, &g_scan_session_template,
		sizeof(session->parent)
	);
	session->item = item;

	lis_bw_item_set_user_ptr(session->bw_item, session);
	*out_session = &session->parent;
	return LIS_OK;
}


static void on_item_close(struct lis_item *item, int root, void *user_data)
{
	struct one_scan_session_private *session;

	LIS_UNUSED(root);
	LIS_UNUSED(user_data);

	session = lis_bw_item_get_user_ptr(item);
	FREE(session);
	lis_bw_item_set_user_ptr(item, NULL);
}



enum lis_error lis_api_workaround_one_page_flatbed(struct lis_api *to_wrap, struct lis_api **impl)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, impl, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	lis_bw_set_on_scan_start(*impl, on_scan_start, NULL);
	lis_bw_set_on_close_item(*impl, on_item_close, NULL);
	return err;
}
