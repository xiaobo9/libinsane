#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libinsane/log.h>
#include <libinsane/normalizers.h>
#include <libinsane/util.h>

#include "../basewrapper.h"
#include "../bmp.h"


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

	bool header_read;
	struct lis_scan_parameters parameters_wrapped;
	struct lis_scan_parameters parameters_out;
	int need_mirroring;

	enum lis_error read_err;

	struct {
		int current; // what has been read in the current line
		int useful; // useful part of the line
		int padding; // extra padding at the end of each line

		uint8_t *content;
	} line;

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


static enum lis_error scan_read_bmp_header(
		struct lis_scan_session *session,
		char *out, size_t bufsize
	)
{
	size_t nb = 0;
	enum lis_error err;

	assert(bufsize > 0);

	while(bufsize > 0 && !session->end_of_page(session)) {
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
			" (remaining: %lu B)",
			(long unsigned)bufsize
		);
		return LIS_ERR_IO_ERROR;
	}
	return LIS_OK;
}


static enum lis_error read_bmp_header(struct lis_bmp2raw_scan_session *private)
{
	enum lis_error err;
	char buffer[BMP_HEADER_SIZE];
	size_t h, nb;
	size_t min_size;

	FREE(private->line.content);
	private->line.current = 0;
	private->line.padding = 0;
	private->line.useful = 0;

	memset(&private->parameters_wrapped, 0, sizeof(private->parameters_wrapped));
	memset(&private->parameters_out, 0, sizeof(private->parameters_out));
	err = private->wrapped->get_scan_parameters(
		private->wrapped, &private->parameters_wrapped
	);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (private->parameters_wrapped.format != LIS_IMG_FORMAT_BMP) {
		lis_log_warning(
			"Unexpected image format: %d. Returning it as is",
			private->parameters_wrapped.format
		);
		return LIS_OK;
	}

	err = scan_read_bmp_header(private->wrapped, buffer, sizeof(buffer));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	h = sizeof(buffer);
	err = lis_bmp2scan_params(buffer, &h, &private->parameters_out);
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (private->parameters_out.height >= 0) {
		// by default, BMP are bottom-to-top, but our RAW24 goes
		// top-to-bottom. So to fix it and keep the scan displayable
		// on-the-fly, we reverse the lines (--> page will appear
		// rotated 180 degrees).
		private->need_mirroring = 1;
	} else {
		private->parameters_out.height *= -1;
	}

	// line length we want in the output
	private->line.useful = private->parameters_out.width * 3;

	// BMP lines must have a number of bytes multiple of 4

	// make sure the size makes sense and uses it to compute padding
	min_size = private->parameters_out.width * private->parameters_out.height * 3;
	lis_log_info(
		"[BMP] Min size: %d ; header says: %d",
		(int)min_size, (int)private->parameters_out.image_size
	);
	if (min_size <= private->parameters_out.image_size) {
		private->line.padding = (
			(private->parameters_out.image_size - min_size)
			/ private->parameters_out.height
		);
		lis_log_info(
			"[BMP] Using BMP size to compute padding: %d",
			(int)private->line.padding
		);
	}

	if (min_size > private->parameters_out.image_size
			|| private->line.padding >= 4
			|| private->line.padding < 0) {
		// line length we have in the BMP
		private->line.padding = (private->parameters_out.width * 3) % 4; // padding
		if (private->line.padding != 0) {
			private->line.padding = 4 - private->line.padding;
		}
		lis_log_info(
			"[BMP] Using line lengths to compute padding: %d",
			(int)private->line.padding
		);
	}

	// we will read line by line ; we need somewhere to store the lines
	private->line.content = calloc(
		sizeof(uint8_t),
		private->line.useful + private->line.padding
	);
	if (private->line.content == NULL) {
		return LIS_ERR_NO_MEM;
	}
	// mark the current content as used (will force loading the next
	// line next time read() is called)
	private->line.current = private->line.useful;

	lis_log_info(
		"BMP: useful line length = %d B ; padding = %d B",
		private->line.useful, private->line.padding
	);

	// lis_bmp2scan_params() returns the image size as stored in the BMP
	// but here we want the image size as RAW24
	private->parameters_out.image_size = (
		3 * private->parameters_out.width
		* private->parameters_out.height
	);

	h -= BMP_HEADER_SIZE;

	if (h > 0) {
		lis_log_info("Extra BMP header: %lu B", (long unsigned)h);
	}

	while(h > 0) {
		// drop any extra BMP header
		nb = h;
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
	FREE(private->line.content);
	lis_bmp2raw_cancel(&private->parent);
}


static int lis_bmp2raw_end_of_feed(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	int r;
	r = private->wrapped->end_of_feed(private->wrapped);
	if (r) {
		FREE(private->line.content);
	}
	return r;
}


static int lis_bmp2raw_end_of_page(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	int r;

	if (private->line.current < private->line.useful) {
		return 0;
	}

	r = private->wrapped->end_of_page(private->wrapped);

	if (r && !private->header_read &&
			!private->parent.end_of_feed(&private->parent)) {
		private->read_err = read_bmp_header(private);
		if (LIS_IS_ERROR(private->read_err)) {
			lis_log_error(
				"Failed to read BMP header: 0x%X, %s",
				private->read_err,
				lis_strerror(private->read_err)
			);
		}
		// avoid double header read
		private->header_read = 1;
	}

	return r;
}


static enum lis_error read_next_line(struct lis_bmp2raw_scan_session *private)
{
	size_t to_read, r;
	enum lis_error err;
	uint8_t *out;

	to_read = private->line.useful + private->line.padding;
	out = private->line.content;
	lis_log_debug("Reading BMP line: %d bytes", (int)to_read);

	while(to_read > 0) {
		r = to_read;
		err = private->wrapped->scan_read(private->wrapped, out, &r);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
		to_read -= r;
		out += r;
	}

	return LIS_OK;
}


static void bgr2rgb(uint8_t *line, int line_len)
{
	uint8_t tmp;

	for (; line_len > 0 ; line += 3, line_len -= 3) {
		tmp = line[0];
		line[0] = line[2];
		line[2] = tmp;
	}
}


static inline void swap_pixels(uint8_t *pa, uint8_t *pb)
{
	uint8_t tmp[3];

	assert(pa != pb);

	tmp[0] = pa[0];
	tmp[1] = pa[1];
	tmp[2] = pa[2];
	pa[0] = pb[0];
	pa[1] = pb[1];
	pa[2] = pb[2];
	pb[0] = tmp[0];
	pb[1] = tmp[1];
	pb[2] = tmp[2];
}


static void mirror_line(uint8_t *line, int line_len)
{
	int pos;

	for (pos = 0; pos < (line_len / 2) - 3 ; pos += 3) {
		swap_pixels(&line[pos], &line[line_len - pos - 3]);
	}
}


static enum lis_error lis_bmp2raw_scan_read(
		struct lis_scan_session *session,
		void *out_buffer, size_t *buffer_size
	)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	enum lis_error err;
	size_t remaining_to_read = *buffer_size;
	size_t to_copy;

	if (LIS_IS_ERROR(private->read_err)) {
		lis_log_warning(
			"Delayed error: 0x%X, %s",
			private->read_err, lis_strerror(private->read_err)
		);
		return private->read_err;
	}

	// indicate to end_of_page() that it will have to read again the
	// next header of the next page
	private->header_read = 0;

	while(remaining_to_read > 0) {
		if (private->line.current >= private->line.useful) {
			if (session->end_of_page(session)) {
				lis_log_debug("scan_read(): end of page");
				*buffer_size -= remaining_to_read;
				return LIS_OK;
			}

			err = read_next_line(private);
			if (LIS_IS_ERROR(err)) {
				lis_log_error(
					"scan_read(): failed to read next"
					" pixel line: 0x%X, %s",
					err, lis_strerror(err)
				);
				return err;
			}
			bgr2rgb(
				private->line.content, private->line.useful
			);
			if (private->need_mirroring) {
				mirror_line(
					private->line.content,
					private->line.useful
				);
			}
			private->line.current = 0;
		}

		to_copy = MIN(
			private->line.useful - private->line.current,
			(int)remaining_to_read
		);
		assert(to_copy > 0);
		memcpy(
			out_buffer,
			private->line.content + private->line.current,
			to_copy
		);
		out_buffer = ((uint8_t *)out_buffer) + to_copy;
		remaining_to_read -= to_copy;
		private->line.current += to_copy;
	}

	return LIS_OK;
}


static void lis_bmp2raw_cancel(struct lis_scan_session *session)
{
	struct lis_bmp2raw_scan_session *private = \
		LIS_BMP2RAW_SCAN_SESSION_PRIVATE(session);
	FREE(private->line.content);
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
