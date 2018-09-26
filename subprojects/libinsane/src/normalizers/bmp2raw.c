#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"
#include "bmp.h"


#define NAME "bmp2raw"


static enum lis_error lis_bmp2raw_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *params
);
static int lis_bmp2raw_end_of_feed(struct lis_scan_session *session);
static int lis_bmp2raw_end_of_page(struct lis_scan_session *session);
static enum lis_error lis_bmp2raw_scan_read(
	struct lis_scan_session *session, void *out_buffer,
	size_t *buffer_size
);
static void lis_bmp2raw_cancel(struct lis_scan_session *session);


struct lis_bmp2raw_scan_session
{
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_item *item;

	struct lis_scan_parameters parameters_wrapped;
	struct lis_scan_parameters parameters_out;

	enum lis_error read_err;
};
#define LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session) \
	((struct lis_bmp2raw_scan_session *)(session))


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = lis_bmp2raw_get_scan_parameters,
	.end_of_feed = lis_bmp2raw_end_of_feed,
	.end_of_page = lis_bmp2raw_end_of_page,
	.scan_read = lis_bmp2raw_scan_read,
	.cancel = lis_bmp2raw_cancel,
};


static enum lis_error scan_read(
		struct lis_scan_session *session,
		char *out, size_t bufsize
	)
{
	size_t nb = 0;
	enum lis_error err;

	while(bufsize > 0
		&& !session->end_of_page(session)
		&& !session->end_of_page(session)) {

		nb = bufsize;
		err = session->scan_read(session, out, &nb);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to read BMP header: 0x%X, %s"
				" (already read: %lu B)",
				err, lis_strerror(err),
				(long unsigned)nb
			);
			return err;
		}

		bufsize -= nb;
		out += nb;
	}

	if (bufsize > 0) {
		lis_log_error(
			"Failed to read BMP header: unexpected EOF"
			" (remaining: %lu B",
			(long unsigned)bufsize
		);
		return LIS_ERR_INTERNAL_IMG_FORMAT_NOT_SUPPORTED;
	}
	return LIS_OK;
}


static enum lis_error read_bmp_header(struct lis_bmp2raw_scan_session *private)
{
	enum lis_error err;
	char buffer[BMP_HEADER_SIZE];
	size_t h, nb;

	err = scan_read(private->wrapped, buffer, sizeof(buffer));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	h = sizeof(buffer);
	err = lis_bmp2scan_params(
		buffer, &h, &private->parameters_out
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	h -= BMP_HEADER_SIZE;

	if (h > 0) {
		lis_log_info("Extra BMP header: %lu B", (long unsigned)h);
	}

	while(h > 0) {
		// drop any extra BMP header
		nb = MIN(h, BMP_HEADER_SIZE);
		err = private->wrapped->scan_read(private->wrapped, buffer, &nb);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to read extra BMP header: 0x%X, %s"
				" (remaining to read: %lu B)",
				err, lis_strerror(err), (long unsigned)h
			);
			return err;
		}
		h -= nb;
	}

	return err;
}


static enum lis_error bmp2raw_scan_start(
		struct lis_item *item, struct lis_scan_session **out,
		void *user_data
	)
{
	struct lis_item *original = lis_bw_get_original_item(item);
	struct lis_item *root = lis_bw_get_root_item(item);
	struct lis_bmp2raw_scan_session *private;
	enum lis_error err;

	LIS_UNUSED(user_data);

	private = lis_bw_item_get_user_ptr(root);
	if (private == NULL) {
		FREE(private);
	}

	private = calloc(1, sizeof(struct lis_bmp2raw_scan_session));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	private->read_err = LIS_OK;

	err = original->scan_start(original, &private->wrapped);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"scan_start() failed: 0x%X, %s",
			err, lis_strerror(err)
		);
		FREE(private);
		return err;
	}
	memcpy(&private->parent, &g_scan_session_template,
		sizeof(private->parent));
	private->item = root;

	err = private->wrapped->get_scan_parameters(
		private->wrapped, &private->parameters_wrapped
	);
	if (LIS_IS_ERROR(err)) {
		private->wrapped->cancel(private->wrapped);
		FREE(private);
		return err;
	}

	if (private->parameters_wrapped.format != LIS_IMG_FORMAT_BMP) {
		lis_log_warning(
			"Unexpected image format: %d. Returning it as is",
			private->parameters_wrapped.format
		);
		*out = private->wrapped;
		FREE(private);
		return LIS_OK;
	}

	err = read_bmp_header(private);
	if (LIS_IS_ERROR(err)) {
		private->wrapped->cancel(private->wrapped);
		FREE(private);
		return err;
	}

	lis_bw_item_set_user_ptr(root, private);
	*out = &private->parent;
	return err;
}


static enum lis_error lis_bmp2raw_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(self);

	memcpy(params, &private->parameters_out, sizeof(*params));
	return LIS_OK;
}


static void bmp2raw_on_item_close(
		struct lis_item *item, int root, void *user_data
	)
{
	struct lis_bmp2raw_scan_session *private;

	LIS_UNUSED(user_data);

	if (!root) {
		return;
	}

	private = lis_bw_item_get_user_ptr(item);
	if (private == NULL) {
		return;
	}

	lis_log_warning(
		"Device has been closed but scan session hasn't been"
		" cancelled"
	);
	lis_bmp2raw_cancel(&private->parent);
}


static int lis_bmp2raw_end_of_feed(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->end_of_feed(private->wrapped);
}


static int lis_bmp2raw_end_of_page(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	int r;

	r = private->wrapped->end_of_page(private->wrapped);

	if (r && !private->parent.end_of_feed(&private->parent)) {
		private->read_err = read_bmp_header(private);
		if (LIS_IS_ERROR(private->read_err)) {
			lis_log_error(
				"Failed to read BMP header: 0x%X, %s",
				private->read_err,
				lis_strerror(private->read_err)
			);
		}
	}

	return r;
}


static enum lis_error lis_bmp2raw_scan_read(
		struct lis_scan_session *session,
		void *out_buffer, size_t *buffer_size
	)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);

	if (LIS_IS_ERROR(private->read_err)) {
		lis_log_warning(
			"Delayed error: 0x%X, %s",
			private->read_err, lis_strerror(private->read_err)
		);
		return private->read_err;
	}

	// BMP is basically RAW with an extra annoying header
	return private->wrapped->scan_read(
		private->wrapped, out_buffer, buffer_size
	);
}


static void lis_bmp2raw_cancel(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	private->wrapped->cancel(private->wrapped);
	lis_bw_item_set_user_ptr(private->item, NULL);
	FREE(private);
}


enum lis_error lis_api_normalizer_bmp2raw(
		struct lis_api *to_wrap, struct lis_api **api
	)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, api, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	lis_bw_set_on_close_item(*api, bmp2raw_on_item_close, NULL);
	lis_bw_set_on_scan_start(*api, bmp2raw_scan_start, NULL);

	return err;
}
