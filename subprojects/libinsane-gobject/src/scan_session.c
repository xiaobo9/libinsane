#include <libinsane/log.h>
#include <libinsane/util.h>

#include <libinsane-gobject/error.h>
#include <libinsane-gobject/error_private.h>
#include <libinsane-gobject/scan_parameters.h>
#include <libinsane-gobject/scan_parameters_private.h>
#include <libinsane-gobject/scan_session.h>
#include <libinsane-gobject/scan_session_private.h>


struct _LibinsaneScanSessionPrivate
{
	struct lis_scan_session *session;
	int finished;
};

G_DEFINE_TYPE_WITH_PRIVATE(LibinsaneScanSession, libinsane_scan_session, G_TYPE_OBJECT)


static void libinsane_scan_session_finalize(GObject *self)
{
	LibinsaneScanSession *session = LIBINSANE_SCAN_SESSION(self);
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(session);
	lis_log_debug("[gobject] Finalizing");
	if (!private->finished) {
		private->session->cancel(private->session);
		private->finished = 1;
	}
}


static void libinsane_scan_session_class_init(LibinsaneScanSessionClass *cls)
{
	GObjectClass *go_cls;
	go_cls = G_OBJECT_CLASS(cls);
	go_cls->finalize = libinsane_scan_session_finalize;
}


static void libinsane_scan_session_init(LibinsaneScanSession *self)
{
	LIS_UNUSED(self);
	lis_log_debug("[gobject] Initializing");
}

LibinsaneScanSession *libinsane_scan_session_new_from_libinsane(
		struct lis_scan_session *scan_session
	)
{
	LibinsaneScanSession *session;
	LibinsaneScanSessionPrivate *private;

	lis_log_debug("[gobject] enter");
	session = g_object_new(LIBINSANE_SCAN_SESSION_TYPE, NULL);
	private = libinsane_scan_session_get_instance_private(session);
	private->session = scan_session;
	private->finished = 0;
	lis_log_debug("[gobject] leave");

	return session;
}


/**
 * libinsane_scan_session_get_scan_parameters:
 * Returns: (transfer full): item scan parameters.
 */
LibinsaneScanParameters *libinsane_scan_session_get_scan_parameters(LibinsaneScanSession *self, GError **error)
{
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(self);
	struct lis_scan_parameters lis_params;
	enum lis_error err;
	LibinsaneScanParameters *params;

	lis_log_debug("enter");
	err = private->session->get_scan_parameters(private->session, &lis_params);
	if (LIS_IS_ERROR(err)) {
		SET_LIBINSANE_GOBJECT_ERROR(error, err,
			"Libinsane scan_session->get_scan_parameters() error: 0x%X, %s",
			err, lis_strerror(err));
		lis_log_debug("error");
		return NULL;
	}

	params = libinsane_scan_parameters_new_from_libinsane(&lis_params);
	lis_log_debug("leave");

	return params;
}


gboolean libinsane_scan_session_end_of_feed(LibinsaneScanSession *self)
{
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(self);
	return private->session->end_of_feed(private->session) > 0;
}


gboolean libinsane_scan_session_end_of_page(LibinsaneScanSession *self)
{
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(self);
	return private->session->end_of_page(private->session) > 0;
}


/**
 * libinsane_scan_session_read:
 * @self: Scan session
 * @buffer: (array length=lng) (element-type guint8): buffer to read data
 * @lng: length of buffer
 * @error: location to store the error if any occurs
 *
 * You must call libinsane_scan_session_end_of_feed() and libinsane_scan_session_end_of_page()
 * after each call to this function before calling it again.
 *
 * Returns: Number of bytes read, or -1 on error
 */
gssize libinsane_scan_session_read(LibinsaneScanSession *self, void *buffer, gsize lng, GError **error)
{
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(self);
	size_t buf_length = lng;
	enum lis_error err;

	lis_log_debug("enter");
	err = private->session->scan_read(private->session, buffer, &buf_length);
	if (LIS_IS_ERROR(err)) {
		SET_LIBINSANE_GOBJECT_ERROR(error, err,
			"Libinsane scan_session->read() error: 0x%X, %s",
			err, lis_strerror(err));
		lis_log_debug("error");
		return -1;
	}
	lis_log_debug("leave: %lu bytes", (long unsigned)buf_length);
	return buf_length;
}


/**
 * libinsane_scan_session_read_bytes:
 * @self: scan session
 * @lng: number of bytes wanted
 * @error: set if an error occurs
 *
 * Returns: (transfer full): a new #GBytes, or %NULL if an error occured
 */
GBytes *libinsane_scan_session_read_bytes(LibinsaneScanSession *self, gsize lng, GError **error)
{
	guchar *buf;
	gssize nread;

	buf = g_malloc(lng);
	nread = libinsane_scan_session_read(self, buf, lng, error);
	if (nread < 0) {
		g_free(buf);
		return NULL;
	} else if (nread == 0) {
		g_free(buf);
		return g_bytes_new_static("", 0);
	} else {
		return g_bytes_new_take(buf, nread);
	}
}


void libinsane_scan_session_cancel(LibinsaneScanSession *self)
{
	LibinsaneScanSessionPrivate *private = libinsane_scan_session_get_instance_private(self);
	private->session->cancel(private->session);
	private->finished = 1;
}
