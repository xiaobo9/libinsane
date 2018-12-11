#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>


#include "raw24.h"
#include "../basewrapper.h"


#define NAME "raw24"


static enum lis_error lis_raw24_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *params
);
static int lis_raw24_end_of_feed(struct lis_scan_session *session);
static int lis_raw24_end_of_page(struct lis_scan_session *session);
static enum lis_error lis_raw24_scan_read(
	struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
);
static void lis_raw24_cancel(struct lis_scan_session *session);


struct lis_raw24_scan_session
{
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct lis_item *item;

	struct lis_scan_parameters params;
	int w; /* position in the current line */
};
#define LIS_RAW24_SCAN_SESSION_PRIVATE(session) \
	((struct lis_raw24_scan_session *)(session))


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = lis_raw24_get_scan_parameters,
	.end_of_feed = lis_raw24_end_of_feed,
	.end_of_page = lis_raw24_end_of_page,
	.scan_read = lis_raw24_scan_read,
	.cancel = lis_raw24_cancel,
};


static enum lis_error raw24_scan_start(
		struct lis_item *item, struct lis_scan_session **out,
		void *user_data
	)
{
	struct lis_item *original = lis_bw_get_original_item(item);
	struct lis_item *root = lis_bw_get_root_item(item);
	struct lis_raw24_scan_session *private;
	enum lis_error err;

	LIS_UNUSED(user_data);

	private = lis_bw_item_get_user_ptr(root);
	if (private == NULL) {
		FREE(private);
	}

	private = calloc(1, sizeof(struct lis_raw24_scan_session));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

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

	// grab the current parameters: see raw24_get_scan_parameters()
	err = private->parent.get_scan_parameters(
		&private->parent, &private->params);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"get_scan_parameters() failed: 0x%X, %s",
			err, lis_strerror(err)
		);
		return err;
	}

	lis_bw_item_set_user_ptr(root, private);

	*out = &private->parent;
	return err;
}


static enum lis_error lis_raw24_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *params
	)
{
	enum lis_error err;
	struct lis_raw24_scan_session *private = \
		LIS_RAW24_SCAN_SESSION_PRIVATE(self);

	err = private->wrapped->get_scan_parameters(
		private->wrapped, params
	);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"get_scan_parameters() failed: 0x%X, %s",
			err, lis_strerror(err)
		);
		return err;
	}

	if (&private->params != params) {
		memcpy(
			&private->params, params,
			sizeof(private->params)
		);
	}

	switch(params->format) {
		case LIS_IMG_FORMAT_RAW_RGB_24:
			return LIS_OK;
		case LIS_IMG_FORMAT_GRAYSCALE_8:
			lis_log_info(
				"Will automatically convert from"
				" grayscale to RGB"
			);
			params->format = LIS_IMG_FORMAT_RAW_RGB_24;
			params->image_size *= 3;
			if (private != NULL) {
				private->params.image_size *= 3;
			}
			return LIS_OK;
		case LIS_IMG_FORMAT_BW_1:
			lis_log_info(
				"Will automatically convert from"
				" B&W to RGB"
			);
			params->format = LIS_IMG_FORMAT_RAW_RGB_24;
			params->image_size *= 8 * 3;
			if (private != NULL) {
				private->params.image_size *= 3;
			}
			return LIS_OK;
		default:
			break;
	}

	lis_log_warning("Unsupported image format: %d", params->format);
	return LIS_OK;
}


static void raw24_on_item_close(
		struct lis_item *item, int root, void *user_data
	)
{
	struct lis_raw24_scan_session *private;

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
	lis_raw24_cancel(&private->parent);
}


static int lis_raw24_end_of_feed(struct lis_scan_session *session)
{
	struct lis_raw24_scan_session *private = \
		LIS_RAW24_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->end_of_feed(private->wrapped);
}


static int lis_raw24_end_of_page(struct lis_scan_session *session)
{
	struct lis_raw24_scan_session *private = \
		LIS_RAW24_SCAN_SESSION_PRIVATE(session);
	return private->wrapped->end_of_page(private->wrapped);
}


void unpack_8_to_24(void *_out_buffer, size_t *out_buffer_size)
{
	uint8_t *out_buffer = _out_buffer;
	ssize_t buffer_size = *out_buffer_size;
	uint8_t val;

	for (buffer_size-- ; buffer_size >= 0 ; buffer_size--) {
		val = out_buffer[buffer_size];
		out_buffer[buffer_size * 3] = val;
		out_buffer[buffer_size * 3 + 1] = val;
		out_buffer[buffer_size * 3 + 2] = val;
	}

	*out_buffer_size *= 3;
}


void unpack_1_to_24(void *_out_buffer, size_t *out_buffer_size)
{
	uint8_t *out_buffer = _out_buffer;
	ssize_t buffer_size = *out_buffer_size;
	int bit;
	uint8_t b;
	uint8_t val;

	for (buffer_size-- ; buffer_size >= 0 ; buffer_size--) {
		b = out_buffer[buffer_size];
		for (bit = 0 ; bit < 8 ; bit++) {
			val = (b & (1 << (7 - bit))) ? 0x00 : 0xFF;
			out_buffer[buffer_size * (3 * 8) + (bit * 3)] = val;
			out_buffer[buffer_size * (3 * 8) + (bit * 3) + 1] = val;
			out_buffer[buffer_size * (3 * 8) + (bit * 3) + 2] = val;
		};
	}

	*out_buffer_size *= 8 * 3;
}


static enum lis_error raw8_scan_read(
		struct lis_raw24_scan_session *private,
		void *out_buffer, size_t *out_buffer_size
	)
{
	enum lis_error err;

	if (*out_buffer_size < 3) {
		lis_log_warning(
			"Buffer too small (%luB < 3), Cannot unpack raw8",
			(long unsigned)*out_buffer_size
		);
		*out_buffer_size = 0;
		return LIS_OK; // hope for a bigger one next time
	}

	*out_buffer_size /= 3;
	err = private->wrapped->scan_read(
		private->wrapped, out_buffer, out_buffer_size
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	unpack_8_to_24(out_buffer, out_buffer_size);
	return err;
}


static enum lis_error raw1_scan_read(
		struct lis_raw24_scan_session *private,
		void *out_buffer, size_t *out_buffer_size
	)
{
	enum lis_error err;
	size_t buflen, nb_pixels;
	size_t out_buflen;

	// round the buffer size to a more convenient size
	*out_buffer_size -= ((*out_buffer_size) % (8 * 3));

	if (*out_buffer_size < 8 * 3) {
		lis_log_error(
			"Buffer too small (%ldB < 24), Cannot unpack raw8",
			(long)*out_buffer_size
		);
		*out_buffer_size = 0;
		return LIS_OK; // hope for a bigger one next time
	}

	if (private->w >= private->params.width) {
		private->w = private->params.width;
	}

	// compute how many bytes we would need to read up to the end of
	// the current pixel line.
	nb_pixels = buflen = private->params.width - private->w;
	if (buflen % 8 != 0) {
		buflen += 8 - (buflen % 8); // round up to upper 8 pixels multiple
	}
	if (buflen > (*out_buffer_size / 3)) {
		// buffer is too short --> can only read partial line
		nb_pixels = buflen = (*out_buffer_size / 3);
	}

	// compute how many bytes we have to read (8 pixels per byte)
	// in input to get the expected number of pixels
	assert(buflen % 8 == 0);
	buflen /= 8;

	lis_log_info("scan_read(): Input buffer = %lu B",
		(long unsigned) *out_buffer_size);
	lis_log_info("scan_read(): Actually requested = %lu B",
		(long unsigned) buflen);
	lis_log_info("scan_read(): Expected nb of pixels = %lu",
		(long unsigned) nb_pixels);

	out_buflen = buflen;
	err = private->wrapped->scan_read(
		private->wrapped, out_buffer, &out_buflen
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	assert(out_buflen <= buflen);


	if (out_buflen < buflen) {
		nb_pixels = out_buflen * 8;
	}
	lis_log_info(
		"scan_read(): Got %lu B --> %lu pixels",
		(long unsigned)out_buflen, (long unsigned)nb_pixels
	);

	unpack_1_to_24(out_buffer, &out_buflen);
	*out_buffer_size = nb_pixels * 3;
	return err;
}


static enum lis_error lis_raw24_scan_read(
		struct lis_scan_session *session,
		void *out_buffer, size_t *buffer_size
	)
{
	struct lis_raw24_scan_session *private = \
		LIS_RAW24_SCAN_SESSION_PRIVATE(session);

	switch(private->params.format) {
		case LIS_IMG_FORMAT_RAW_RGB_24:
			break;
		case LIS_IMG_FORMAT_GRAYSCALE_8:
			return raw8_scan_read(
				private, out_buffer, buffer_size
			);
		case LIS_IMG_FORMAT_BW_1:
			return raw1_scan_read(
				private, out_buffer, buffer_size
			);
		default:
			break;
	}

	// return the content as it. Get_scan_parameters() has already
	// raised a warning if required.
	return private->wrapped->scan_read(
		private->wrapped, out_buffer, buffer_size
	);
}


static void lis_raw24_cancel(struct lis_scan_session *session)
{
	struct lis_raw24_scan_session *private = \
		LIS_RAW24_SCAN_SESSION_PRIVATE(session);
	private->wrapped->cancel(private->wrapped);
	lis_bw_item_set_user_ptr(private->item, NULL);
	FREE(private);
}


enum lis_error lis_api_normalizer_raw24(
		struct lis_api *to_wrap, struct lis_api **api
	)
{
	enum lis_error err;

	err = lis_api_base_wrapper(to_wrap, api, NAME);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	lis_bw_set_on_close_item(*api, raw24_on_item_close, NULL);
	lis_bw_set_on_scan_start(*api, raw24_scan_start, NULL);

	return err;
}
