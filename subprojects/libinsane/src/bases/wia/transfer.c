#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "../../bmp.h"
#include "properties.h"
#include "transfer.h"
#include "util.h"


#define lis_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define lis_container_of(ptr, type, member) \
      ((type *) ((char *)(ptr) - lis_offsetof(type, member)))


enum wia_msg_type {
	WIA_MSG_DATA,
	WIA_MSG_END_OF_PAGE,
	WIA_MSG_END_OF_FEED,
	WIA_MSG_ERROR,
};


struct wia_msg {
	struct wia_msg *next;

	enum wia_msg_type type;

	union {
		enum lis_error error; // only if WIA_MSG_ERROR
		size_t length; // only if WIA_MSG_DATA
	} content;

	char data[]; // only if WIA_MSG_DATA
};


struct wia_transfer {
	LisIWiaItem2 *wia_item;
	IWiaPropertyStorage *wia_props;

	struct lis_scan_session scan_session;

	int refcount;

	IStream istream;
	LisIWiaAppErrorHandler app_error_handler;
	LisIWiaTransferCallback transfer_callback;

	int end_of_feed;

	struct {
		HANDLE thread;
		long written;
		long read;

		CRITICAL_SECTION critical_section;
		CONDITION_VARIABLE condition_variable;

		struct wia_msg *first;
		struct wia_msg *last;
	} scan;

	struct {
		struct wia_msg *msg;
		size_t already_read;
	} current;
};


static enum lis_error get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *parameters
);
static int end_of_feed(struct lis_scan_session *session);
static int end_of_page(struct lis_scan_session *session);
static enum lis_error scan_read (
	struct lis_scan_session *session, void *out_buffer, size_t *buffer_size
);
static void scan_cancel(struct lis_scan_session *session);


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = get_scan_parameters,
	.end_of_feed = end_of_feed,
	.end_of_page = end_of_page,
	.scan_read = scan_read,
	.cancel = scan_cancel,
};


static HRESULT WINAPI wia_transfer_cb_query_interface(
	LisIWiaTransferCallback *self, REFIID riid, void **ppvObject
);
static ULONG WINAPI wia_transfer_cb_add_ref(LisIWiaTransferCallback *self);
static ULONG WINAPI wia_transfer_cb_release(LisIWiaTransferCallback *self);
static HRESULT WINAPI wia_transfer_cb_transfer_callback(
	LisIWiaTransferCallback *self, LONG lFlags,
	LisWiaTransferParams *pWiaTransferParams
);
static HRESULT WINAPI wia_transfer_cb_get_next_stream(
	LisIWiaTransferCallback *self,
	LONG lFlags,
	BSTR bstrItemName,
	BSTR bstrFullItemName,
	IStream **ppDestination
);

static LisIWiaTransferCallbackVtbl g_wia_transfer_callback = {
	.QueryInterface = wia_transfer_cb_query_interface,
	.AddRef = wia_transfer_cb_add_ref,
	.Release = wia_transfer_cb_release,
	.TransferCallback = wia_transfer_cb_transfer_callback,
	.GetNextStream = wia_transfer_cb_get_next_stream,
};


static HRESULT WINAPI wia_stream_query_interface(
	IStream *_self,
	REFIID riid,
	void **ppvObject
);
static ULONG WINAPI wia_stream_add_ref(IStream *_self);
static ULONG WINAPI wia_stream_release(IStream *_self);
static HRESULT WINAPI wia_stream_read(
	IStream *_self,
	void *pv,
	ULONG cb,
	ULONG *pcbRead
);
static HRESULT WINAPI wia_stream_write(
	IStream *_self,
	const void *pv,
	ULONG cb,
	ULONG *pcbWritten
);
static HRESULT WINAPI wia_stream_seek(
	IStream *_self,
	LARGE_INTEGER dlibMove,
	DWORD dwOrigin,
	ULARGE_INTEGER *plibNewPosition
);
static HRESULT WINAPI wia_stream_set_size(
	IStream *_self,
	ULARGE_INTEGER libNewSize
);
static HRESULT WINAPI wia_stream_copy_to(
	IStream *_self,
	IStream *pstm,
	ULARGE_INTEGER cb,
	ULARGE_INTEGER *pcbRead,
	ULARGE_INTEGER *pcbWritten
);
static HRESULT WINAPI wia_stream_commit(
	IStream *_self,
	DWORD grfCommitFlags
);
static HRESULT WINAPI wia_stream_revert(
	IStream *_self
);
static HRESULT WINAPI wia_stream_lock_region(
	IStream *_self,
	ULARGE_INTEGER libOffset,
	ULARGE_INTEGER cb,
	DWORD dwLockType
);
static HRESULT WINAPI wia_stream_unlock_region(
	IStream *_self,
	ULARGE_INTEGER libOffset,
	ULARGE_INTEGER cb,
	DWORD dwLockType
);
static HRESULT WINAPI wia_stream_stat(
	IStream *_self,
	STATSTG *pstatstg,
	DWORD grfStatFlag
);
static HRESULT WINAPI wia_stream_clone(
	IStream *_self,
	IStream **ppstm
);


static IStreamVtbl g_wia_stream = {
	.QueryInterface = wia_stream_query_interface,
	.AddRef = wia_stream_add_ref,
	.Release = wia_stream_release,
	.Read = wia_stream_read,
	.Write = wia_stream_write,
	.Seek = wia_stream_seek,
	.SetSize = wia_stream_set_size,
	.CopyTo = wia_stream_copy_to,
	.Commit = wia_stream_commit,
	.Revert = wia_stream_revert,
	.LockRegion = wia_stream_lock_region,
	.UnlockRegion = wia_stream_unlock_region,
	.Stat = wia_stream_stat,
	.Clone = wia_stream_clone,
};


static HRESULT WINAPI wia_app_error_handler_query_interface(
	LisIWiaAppErrorHandler *self,
	REFIID riid,
	void **ppvObject
);
static ULONG WINAPI wia_app_error_handler_add_ref(LisIWiaAppErrorHandler *self);
static ULONG WINAPI wia_app_error_handler_release(LisIWiaAppErrorHandler *self);
static HRESULT WINAPI wia_app_error_handler_get_window(
	LisIWiaAppErrorHandler *self,
	HWND *phwnd
);
static HRESULT WINAPI wia_app_error_handler_report_status(
	LisIWiaAppErrorHandler *self,
	LONG lFlags,
	LisIWiaItem2 *pWiaItem2,
	HRESULT hrStatus,
	LONG lPercentComplete
);


static LisIWiaAppErrorHandlerVtbl g_wia_app_error_handler = {
	.QueryInterface = wia_app_error_handler_query_interface,
	.AddRef = wia_app_error_handler_add_ref,
	.Release = wia_app_error_handler_release,
	.GetWindow = wia_app_error_handler_get_window,
	.ReportStatus = wia_app_error_handler_report_status,
};


static HRESULT add_msg(
		struct wia_transfer *self,
		enum wia_msg_type type,
		const void *data,
		size_t msg_length
	)
{
	struct wia_msg *msg;

	if (data == NULL) {
		msg_length = 0;
	}

	msg = GlobalAlloc(GPTR, sizeof(struct wia_msg) + msg_length);
	if (msg == NULL) {
		lis_log_error("Out of memory");
		return E_OUTOFMEMORY;
	}

	msg->type = type;
	msg->content.length = msg_length;
	msg->next = NULL;
	if (msg_length > 0) {
		memcpy(msg->data, data, msg_length);
	}

	EnterCriticalSection(&self->scan.critical_section);
	if (self->scan.last != NULL) {
		assert(self->scan.first != NULL);
		self->scan.last->next = msg;
	} else {
		assert(self->scan.first == NULL);
		self->scan.first = msg;
	}
	self->scan.last = msg;
	LeaveCriticalSection(&self->scan.critical_section);

	WakeConditionVariable(&self->scan.condition_variable);

	return S_OK;
}


static HRESULT add_error(
		struct wia_transfer *self,
		enum lis_error error
	)
{
	struct wia_msg *msg;

	msg = GlobalAlloc(GPTR, sizeof(struct wia_msg));
	if (msg == NULL) {
		lis_log_error("Out of memory");
		return E_OUTOFMEMORY;
	}

	msg->type = WIA_MSG_ERROR;
	msg->content.error = error;

	EnterCriticalSection(&self->scan.critical_section);
	if (self->scan.last != NULL) {
		assert(self->scan.first != NULL);
		self->scan.last->next = msg;
	} else {
		assert(self->scan.first == NULL);
		self->scan.first = msg;
	}
	self->scan.last = msg;
	LeaveCriticalSection(&self->scan.critical_section);

	WakeConditionVariable(&self->scan.condition_variable);

	return S_OK;
}


static struct wia_msg *pop_msg(struct wia_transfer *self, bool wait)
{
	struct wia_msg *msg;

	assert(!wait || !self->end_of_feed);

	EnterCriticalSection(&self->scan.critical_section);

	if (self->scan.first == NULL) {
		if (wait) {
			SleepConditionVariableCS(
				&self->scan.condition_variable,
				&self->scan.critical_section,
				INFINITE
			);
		} else {
			LeaveCriticalSection(&self->scan.critical_section);
			return NULL;
		}
	}

	msg = self->scan.first;

	if (msg->next == NULL) {
		assert(msg == self->scan.last);
		self->scan.first = NULL;
		self->scan.last = NULL;
	} else {
		assert(msg != self->scan.last);
		self->scan.first = msg->next;
	}

	LeaveCriticalSection(&self->scan.critical_section);

	if (msg->type == WIA_MSG_END_OF_FEED || msg->type == WIA_MSG_ERROR) {
		self->end_of_feed = 1;
	}

	return msg;
}


static void free_msg(struct wia_msg *msg)
{
#if 1
	if (msg != NULL) {
		GlobalFree(msg);
	}
#else
	LIS_UNUSED(msg);
#endif
}


static void pop_all_msg(struct wia_transfer *self)
{
	struct wia_msg *msg;

	while ((msg = pop_msg(self, FALSE /* wait */)) != NULL) {
		free_msg(msg);
	}
}


static int end_of_feed(struct lis_scan_session *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, scan_session
	);
	int r;

	if (self->end_of_feed || self->scan.thread == NULL) {
		return 1;
	}

	while(self->current.msg == NULL
			|| self->current.msg->type == WIA_MSG_END_OF_PAGE) {
		free_msg(self->current.msg);
		self->current.msg = pop_msg(self, TRUE /* wait */);
		self->current.already_read = 0;
	}

	r = (self->current.msg->type == WIA_MSG_END_OF_FEED);
	lis_log_debug("end_of_feed() = %d", r);
	if (r) {
		lis_log_info("Read by app: %ld B", self->scan.read);
		self->scan.read = 0;
	}
	return r;
}


static int end_of_page(struct lis_scan_session *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, scan_session
	);
	int r;

	if (self->end_of_feed || self->scan.thread == NULL) {
		return 1;
	}

	if (self->current.msg == NULL) {
		self->current.msg = pop_msg(self, TRUE /* wait */);
		self->current.already_read = 0;
	}

	r = (
		self->current.msg->type == WIA_MSG_END_OF_FEED
		|| self->current.msg->type == WIA_MSG_END_OF_PAGE
	);
	lis_log_debug("end_of_page() = %d", r);
	if (r) {
		lis_log_info("Read by app: %ld B", self->scan.read);
		self->scan.read = 0;
	}
	return r;
}


static enum lis_error scan_read(
		struct lis_scan_session *_self, void *out_buffer,
		size_t *buffer_size
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, scan_session
	);

	lis_log_debug("scan_read() ...");

	if (self->end_of_feed || self->scan.thread == NULL) {
		lis_log_error("scan_read(): End of feed. Shouldn't be called");
		return LIS_ERR_INVALID_VALUE;
	}

	while (self->current.msg == NULL
			|| self->current.msg->type == WIA_MSG_END_OF_PAGE) {
		free_msg(self->current.msg);
		self->current.already_read = 0;
		self->current.msg = pop_msg(self, TRUE /* wait */);
	}

	switch(self->current.msg->type) {

		case WIA_MSG_DATA:
			assert(self->current.msg->content.length > self->current.already_read);
			*buffer_size = MIN(
				*buffer_size,
				self->current.msg->content.length - self->current.already_read
			);
			self->scan.read += (*buffer_size);
			memcpy(
				out_buffer,
				self->current.msg->data + self->current.already_read,
				*buffer_size
			);
			self->current.already_read += *buffer_size;
			if (self->current.already_read >= self->current.msg->content.length) {
				free_msg(self->current.msg);
				self->current.msg = NULL;
				self->current.already_read = 0;
			};
			lis_log_debug("scan_read(): Got %ld bytes", (long)(*buffer_size));
			return LIS_OK;

		case WIA_MSG_END_OF_PAGE:
			assert(FALSE); // can't / mustn't happen
			break;

		case WIA_MSG_END_OF_FEED:
			self->end_of_feed = 1;
			self->scan.read = 0;
			lis_log_error("scan_read(): Unexpected end of feed");
			return LIS_ERR_INVALID_VALUE;

		case WIA_MSG_ERROR:
			lis_log_error(
				"scan_read(): Remote error: 0x%X, %s",
				self->current.msg->content.error,
				lis_strerror(self->current.msg->content.error)
			);
			return self->current.msg->content.error;

	}

	lis_log_error(
		"scan_read(): Unknown message type: %d",
		self->current.msg->type
	);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}


static enum lis_error get_scan_parameters(
		struct lis_scan_session *_self,
		struct lis_scan_parameters *parameters
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, scan_session
	);
	static const PROPSPEC input[] = {
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPS_XPOS,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPS_XEXTENT,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPS_YPOS,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPS_YEXTENT,
		},
		{
			.ulKind = PRSPEC_PROPID,
			.propid = WIA_IPA_FORMAT,
		},
	};
	PROPVARIANT output[LIS_COUNT_OF(input)] = { 0 };
	unsigned int i;
	HRESULT hr;
	const struct lis_wia2lis_property *img_format_opt;
	enum lis_error err;
	union lis_value img_format;
	char *tmp = NULL;

	lis_log_debug("get_scan_parameters() ...");

	memset(parameters, 0, sizeof(struct lis_scan_parameters));

	img_format_opt = lis_wia2lis_get_property(
		FALSE /* !root */, WIA_IPA_FORMAT
	);
	assert(img_format_opt != NULL);

	hr = self->wia_props->lpVtbl->ReadMultiple(
		self->wia_props,
		LIS_COUNT_OF(output),
		input, output
	);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		lis_log_error(
			"Failed to get_scan_parameters: 0x%lX -> 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		return err;
	}

	if (output[0].vt != VT_I4 || output[1].vt != VT_I4
			|| output[2].vt != VT_I4 || output[3].vt != VT_I4) {
		lis_log_error(
			"Got unexpected value types for scan frame: %d %d %d %d",
			output[0].vt, output[1].vt, output[2].vt, output[3].vt
		);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}
	if (output[4].vt != VT_CLSID) {
		lis_log_error(
			"Got unexpected value type for scan image format: %d",
			output[4].vt
		);
		return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
	}

	parameters->width = output[1].lVal - output[0].lVal + 1;
	parameters->height = output[3].lVal - output[2].lVal + 1;

	err = lis_convert_wia2lis(img_format_opt, &output[4], &img_format, &tmp);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("get_scan_parameters(): Failed to interpret image format");
		goto end;
	}
	FREE(tmp);
	parameters->format = img_format.format;

	// ASSUMPTION(Jflesch): assuming BMP here:
	parameters->image_size = BMP_HEADER_SIZE + (
		parameters->width * parameters->height * 3
	);

	lis_log_info(
		"WIA: get_scan_parameters(): %ld x %ld + %ld + %ld",
		output[0].lVal, output[2].lVal,
		output[1].lVal, output[3].lVal
	);
	lis_log_info(
		"WIA: get_scan_parameters(): %d x %d (format=%d, image_size=%lu)",
		parameters->width, parameters->height, parameters->format,
		(long)parameters->image_size
	);

	err = LIS_OK;
end:
	for (i = 0 ; i < LIS_COUNT_OF(output) ; i++) {
		PropVariantClear(&output[i]);
	}
	return err;
}


static void scan_cancel(struct lis_scan_session *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, scan_session
	);

	// TODO(Jflesch): Call IWiaTransfer->Cancel()

	if (self->scan.thread != NULL) {
		WaitForSingleObject(self->scan.thread, INFINITE);
		CloseHandle(self->scan.thread);
		pop_all_msg(self);
		free_msg(self->current.msg);
		self->current.msg = NULL;
		self->current.already_read = 0;
		self->scan.thread = NULL;
	}

	self->wia_item->lpVtbl->Release(self->wia_item);
	self->wia_props->lpVtbl->Release(self->wia_props);

	self->istream.lpVtbl->Release(&self->istream);
	self->app_error_handler.lpVtbl->Release(&self->app_error_handler);
	self->transfer_callback.lpVtbl->Release(&self->transfer_callback);
}


static HRESULT WINAPI wia_app_error_handler_query_interface(
		LisIWiaAppErrorHandler *_self,
		REFIID riid,
		void **ppvObject
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);
	HRESULT hr;
	LPOLESTR str;
	char *cstr;

	if (IsEqualIID(riid, &IID_IUnknown)) {
		lis_log_info("WiaAppErrorHandler->QueryInterface(IUnknown)");
		*ppvObject = _self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaAppErrorHandler)) {
		lis_log_info(
			"WiaAppErrorHandler->QueryInterface("
			"IWiaAppErrorHandler)"
		);
		*ppvObject = _self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaTransferCallback)) {
		lis_log_info(
			"WiaAppErrorHandler->QueryInterface("
			"IWiaTransferCallback)"
		);
		*ppvObject = &self->transfer_callback;
		return S_OK;
	} else {
		cstr = NULL;
		hr = StringFromCLSID(riid, &str);
		if (!FAILED(hr)) {
			cstr = lis_bstr2cstr(str);
			CoTaskMemFree(str);
		}
		lis_log_warning(
			"WiaAppErrorHandler->QueryInterface(%s): Unknown interface",
			(cstr != NULL) ? cstr : "NULL"
		);
		FREE(cstr);
	}

	return E_NOTIMPL;
}


static ULONG WINAPI wia_app_error_handler_add_ref(LisIWiaAppErrorHandler *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, app_error_handler
	);

	self->refcount++;
	lis_log_info("WiaAppErrorHandler->AddRef() -> %d", self->refcount);
	return (unsigned long)self->refcount;
}

static void wia_transfer_release(struct wia_transfer *self)
{
	self->refcount--;
	if (self->refcount == 0) {
		lis_log_info("Freeing WIA transfer objects");
		// XXX(JFlesch): Disabled: Some drivers seems to call
		// Release() far too much (?) (Brother MFC-7360N)
		// GlobalFree(self);
	}
}


static ULONG WINAPI wia_app_error_handler_release(LisIWiaAppErrorHandler *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, app_error_handler
	);

	lis_log_info("WiaAppErrorHandler->Release() <- %d", self->refcount);
	wia_transfer_release(self);
	return (unsigned long)self->refcount;
}


static HRESULT WINAPI wia_app_error_handler_get_window(
		LisIWiaAppErrorHandler *self,
		HWND *phwnd
	)
{
	LIS_UNUSED(self);

	lis_log_info("WiaAppErrorHandler->GetWindow()");
	*phwnd = NULL;
	return S_OK;
}


static HRESULT WINAPI wia_app_error_handler_report_status(
		LisIWiaAppErrorHandler *self,
		LONG lFlags,
		LisIWiaItem2 *pWiaItem2,
		HRESULT hrStatus,
		LONG lPercentComplete
	)
{
	LIS_UNUSED(self);
	LIS_UNUSED(lFlags); // unused ; should be 0
	LIS_UNUSED(pWiaItem2);

	lis_log_info(
		"WiaAppErrorHandler->ReportStatus(status=0x%lX, percent=%ld%%)",
		hrStatus, lPercentComplete
	);
	return S_OK;
}


static HRESULT WINAPI wia_transfer_cb_query_interface(
		LisIWiaTransferCallback *_self, REFIID riid, void **ppvObject
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);
	HRESULT hr;
	LPOLESTR str;
	char *cstr;

	if (IsEqualIID(riid, &IID_IUnknown)) {
		lis_log_info("WiaTransferCallback->QueryInterface(IUnknown)");
		*ppvObject = _self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaTransferCallback)) {
		lis_log_info(
			"WiaTransferCallback->QueryInterface("
			"IWiaTransferCallback)"
		);
		*ppvObject = _self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaAppErrorHandler)) {
		lis_log_info(
			"WiaTransferCallback->QueryInterface("
			"IWiaAppErrorHandler)"
		);
		*ppvObject = &self->app_error_handler;
		return S_OK;
	} else {
		cstr = NULL;
		hr = StringFromCLSID(riid, &str);
		if (!FAILED(hr)) {
			cstr = lis_bstr2cstr(str);
			CoTaskMemFree(str);
		}
		lis_log_warning(
			"WiaTransfer->QueryInterface(%s): Unknown interface",
			(cstr != NULL) ? cstr : "NULL"
		);
		FREE(cstr);
	}

	return E_NOTIMPL;
}


static ULONG WINAPI wia_transfer_cb_add_ref(LisIWiaTransferCallback *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);

	self->refcount++;
	lis_log_info("WiaTransfer->AddRef() -> %d", self->refcount);
	return (unsigned long)self->refcount;
}


static ULONG WINAPI wia_transfer_cb_release(LisIWiaTransferCallback *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);

	lis_log_info("WiaTransfer->Release() -> %d", self->refcount);
	wia_transfer_release(self);
	return (unsigned long)self->refcount;
}


static HRESULT WINAPI wia_transfer_cb_transfer_callback(
		LisIWiaTransferCallback *_self, LONG lFlags,
		LisWiaTransferParams *pWiaTransferParams
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);

	const char *msg = "unknown";

	LIS_UNUSED(lFlags); // unused, should be 0

	switch(pWiaTransferParams->lMessage) {
		case LIS_WIA_TRANSFER_MSG_STATUS:
			msg = "status";
			break;
		case LIS_WIA_TRANSFER_MSG_END_OF_STREAM:
			msg = "end_of_stream";
			break;
		case LIS_WIA_TRANSFER_MSG_END_OF_TRANSFER:
			msg = "end_of_transfer";
			break;
		case LIS_WIA_TRANSFER_MSG_DEVICE_STATUS:
			msg = "device_status";
			break;
		case LIS_WIA_TRANSFER_MSG_NEW_PAGE:
			msg = "new_page";
			break;
	}

	/* XXX(Jflesch):
	 * With Brother MFC-7360N, pWiaTransferParams->hrError appears
	 * to be garbage.
	 */
	lis_log_info(
		"WiaTransfer->TransferCallback("
		"msg=%s, "
		"percent=%ld%%, "
		"transferred=%ld B, "
		"error=0x%lX"
		")",
		msg, pWiaTransferParams->lPercentComplete,
		(long)pWiaTransferParams->ulTransferredBytes,
		pWiaTransferParams->hrErrorStatus
	);

	switch(pWiaTransferParams->lMessage) {
		case LIS_WIA_TRANSFER_MSG_STATUS:
			break;
		case LIS_WIA_TRANSFER_MSG_DEVICE_STATUS:
			break;
		case LIS_WIA_TRANSFER_MSG_END_OF_STREAM:
			return add_msg(
				self, WIA_MSG_END_OF_PAGE,
				NULL /* data */,  0 /* length */
			);
		case LIS_WIA_TRANSFER_MSG_END_OF_TRANSFER:
			return add_msg(
				self, WIA_MSG_END_OF_FEED,
				NULL /* data */,  0 /* length */
			);
		case LIS_WIA_TRANSFER_MSG_NEW_PAGE:
			return add_msg(
				self, WIA_MSG_END_OF_PAGE,
				NULL /* data */,  0 /* length */
			);
		default:
			lis_log_warning(
				"WiaTransfer->TransferCallback("
				"msg=%s, "
				"percent=%ld%%, "
				"transferred=%ld B, "
				"error=0x%lX"
				"): Unknown notification",
				msg, pWiaTransferParams->lPercentComplete,
				(long)pWiaTransferParams->ulTransferredBytes,
				pWiaTransferParams->hrErrorStatus
			);
			break;
	}

	return S_OK;
}


static HRESULT WINAPI wia_stream_query_interface(
		IStream *self,
		REFIID riid,
		void **ppvObject
	)
{
	HRESULT hr;
	LPOLESTR str;
	char *cstr;

	if (IsEqualIID(riid, &IID_IUnknown)) {
		lis_log_info("IStream->QueryInterface(IUnknown)");
		*ppvObject = self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_IStream)) {
		lis_log_info("IStream->QueryInterface(IStream)");
		*ppvObject = self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_IAgileObject)) {
		// EPSON XP-425
		lis_log_info("IStream->QueryInterface(IAgileObject)");
		*ppvObject = self;
		return S_OK;
	} else {
		cstr = NULL;
		hr = StringFromCLSID(riid, &str);
		if (!FAILED(hr)) {
			cstr = lis_bstr2cstr(str);
			CoTaskMemFree(str);
		}
		lis_log_warning(
			"IStream->QueryInterface(%s): Unknown interface",
			(cstr != NULL) ? cstr : "NULL"
		);
		FREE(cstr);
	}

	return E_NOTIMPL;
}


static ULONG WINAPI wia_stream_add_ref(IStream *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, istream
	);

	self->refcount++;
	lis_log_info("IStream->AddRef() -> %d", self->refcount);
	return (unsigned long)self->refcount;
}


static ULONG WINAPI wia_stream_release(IStream *_self)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, istream
	);

	lis_log_info("IStream->Release() <- %d", self->refcount);
	wia_transfer_release(self);
	return (unsigned long)self->refcount;
}


static HRESULT WINAPI wia_stream_read(
		IStream *_self,
		void *pv,
		ULONG cb,
		ULONG *pcbRead
	)
{
	LIS_UNUSED(_self);
	LIS_UNUSED(pv);

	*pcbRead = 0;

	lis_log_warning("IStream->Read(%lu B): Unsupported", cb);

	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_write(
		IStream *_self,
		const void *pv,
		ULONG cb,
		ULONG *pcbWritten
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, istream
	);

	lis_log_info(
		"IStream->Write(%lu B) (total: %ld B)",
		cb, self->scan.written
	);

	*pcbWritten = cb;
	self->scan.written += cb;

	if (cb <= 0) {
		return S_OK;
	}

	return add_msg(self, WIA_MSG_DATA, pv, cb);
}


static HRESULT WINAPI wia_stream_seek(
		IStream *_self,
		LARGE_INTEGER dlibMove,
		DWORD dwOrigin,
		ULARGE_INTEGER *plibNewPosition
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, istream
	);
	const char *origin = "UNKNOWN";

	switch(dwOrigin) {
		case STREAM_SEEK_SET:
			origin = "SET";
			break;
		case STREAM_SEEK_CUR:
			origin = "CURRENT";
			break;
		case STREAM_SEEK_END:
			origin = "END";
			break;
	}

	if (plibNewPosition == NULL) {
		// Canon i-SENSYS MF3010
		lis_log_info(
			"IStream->Seek(%ld, %s, NULL) (written=%ld B)",
			(long)dlibMove.QuadPart, origin,
			self->scan.written
		);
		return S_OK;
	}

	lis_log_info(
		"IStream->Seek(%ld, %s, %lu) (written=%ld B)",
		(long)dlibMove.QuadPart, origin,
		(long)plibNewPosition->QuadPart,
		self->scan.written
	);

	switch(dwOrigin) {
		case STREAM_SEEK_SET:
			if (dlibMove.QuadPart == self->scan.written) {
				/* Epson WorkForce ES-300W */
				/* HP Photosmart C4200 */
				/* Epson Perfection V30/v300 */
				plibNewPosition->QuadPart = dlibMove.QuadPart;
				return S_OK;
			}
			break;
		case STREAM_SEEK_END:
			if (dlibMove.QuadPart == 0) {
				plibNewPosition->QuadPart = self->scan.written;
				return S_OK;
			}
			break;
	}

	lis_log_error(
		"IStream->Seek(%ld, %s, %lu) (written=%ld):"
		" Unsupported operation",
		(long)dlibMove.QuadPart, origin,
		(long)plibNewPosition->QuadPart,
		self->scan.written
	);
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_set_size(
		IStream *_self,
		ULARGE_INTEGER libNewSize
	)
{
	LIS_UNUSED(_self);
	lis_log_error(
		"IStream->SetSize(%lu): Unsupported operation",
		(long)libNewSize.QuadPart
	);
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_copy_to(
		IStream *_self,
		IStream *pstm,
		ULARGE_INTEGER cb,
		ULARGE_INTEGER *pcbRead,
		ULARGE_INTEGER *pcbWritten
	)
{
	LIS_UNUSED(_self);
	LIS_UNUSED(pstm);
	LIS_UNUSED(pcbRead);
	LIS_UNUSED(pcbWritten);

	lis_log_error(
		"IStream->CopyTo(%ld B): Unsupported operation",
		(long)cb.QuadPart
	);
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_commit(IStream *self, DWORD grfCommitFlags)
{
	LIS_UNUSED(self);
	lis_log_info("IStream->Commit(0x%lX)", grfCommitFlags);
	return S_OK;
}


static HRESULT WINAPI wia_stream_revert(IStream *self)
{
	LIS_UNUSED(self);
	lis_log_error("IStream->Revert(): Unsupported operation");
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_lock_region(
		IStream *self,
		ULARGE_INTEGER libOffset,
		ULARGE_INTEGER cb,
		DWORD dwLockType
	)
{
	LIS_UNUSED(self);
	lis_log_warning(
		"IStream->LockRegion(%lu, %lu, 0x%lX)",
		(long)libOffset.QuadPart, (long)cb.QuadPart, dwLockType
	);
	return S_OK;
}


static HRESULT WINAPI wia_stream_unlock_region(
		IStream *self,
		ULARGE_INTEGER libOffset,
		ULARGE_INTEGER cb,
		DWORD dwLockType
	)
{
	LIS_UNUSED(self);
	lis_log_warning(
		"IStream->UnlockRegion(%lu, %lu, 0x%lX)",
		(long)libOffset.QuadPart, (long)cb.QuadPart, dwLockType
	);
	return S_OK;
}


static HRESULT WINAPI wia_stream_stat(
		IStream *_self,
		STATSTG *pstatstg,
		DWORD grfStatFlag
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, istream
	);
	SYSTEMTIME systemTime;
	FILETIME fileTime;

	lis_log_info("IStream->Stat(0x%lX)", grfStatFlag);

	GetSystemTime(&systemTime);
	SystemTimeToFileTime(&systemTime, &fileTime);

	memset(pstatstg, 0, sizeof(STATSTG));

	pstatstg->type = STGTY_STREAM;
	pstatstg->mtime = fileTime;
	pstatstg->atime = fileTime;
	pstatstg->grfLocksSupported = LOCK_EXCLUSIVE;
	pstatstg->cbSize.QuadPart = self->scan.written;
	pstatstg->clsid = CLSID_NULL;
	return S_OK;
}


static HRESULT WINAPI wia_stream_clone(IStream *self, IStream **cloned)
{
	LIS_UNUSED(self);
	LIS_UNUSED(cloned);

	lis_log_error("IStream->Clone(): Unsupported operation");
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_transfer_cb_get_next_stream(
		LisIWiaTransferCallback *_self,
		LONG lFlags,
		BSTR bstrItemName,
		BSTR bstrFullItemName,
		IStream **ppDestination
	)
{
	struct wia_transfer *self = lis_container_of(
		_self, struct wia_transfer, transfer_callback
	);
	char *item_name = NULL;
	char *full_item_name = NULL;

	item_name = lis_bstr2cstr(bstrItemName);
	full_item_name = lis_bstr2cstr(bstrFullItemName);
	lis_log_info(
		"WiaTransfer->GetNextStream(0x%lX, %s, %s)",
		lFlags,
		(item_name != NULL ? item_name : NULL),
		(full_item_name != NULL ? full_item_name : NULL)
	);
	FREE(item_name);
	FREE(full_item_name);

	lis_log_info("Written by WIA driver: %ld B", self->scan.written);
	self->scan.written = 0;
	*ppDestination = &self->istream;
	return S_OK;
}


static DWORD WINAPI thread_download(void *_self)
{
	struct wia_transfer *self = _self;
	LisIWiaTransfer *wia_transfer = NULL;
	HRESULT download_hr, hr;
	enum lis_error err;

	lis_log_info("Starting scan thread ...");
	lis_log_debug("WiaItem->QueryInterfae(WiaTransfer) ...");
	hr = self->wia_item->lpVtbl->QueryInterface(
		self->wia_item,
		&IID_LisWiaTransfer,
		(void **)&wia_transfer
	);
	lis_log_debug("WiaItem->QueryInterface(WiaTransfer): 0x%lX", hr);
	if (FAILED(hr)) {
		err = hresult_to_lis_error(hr);
		add_error(self, err);
		lis_log_error(
			"WiaItem->QueryInterface(WiaTransfer): 0x%lX, 0x%X, %s",
			hr, err, lis_strerror(err)
		);
		goto end;
	}
	lis_log_info("Scan thread running");

	lis_log_debug("WiaTransfer->Download() ...");
	self->scan.written = 0;
	download_hr = wia_transfer->lpVtbl->Download(
		wia_transfer,
		0, /* unused */
		&self->transfer_callback
	);
	lis_log_debug(
		"WiaTransfer->Download(): 0x%lX",
		download_hr
	);
	lis_log_info("Written by WIA driver: %ld B", self->scan.written);

	if (!FAILED(download_hr)) {
		lis_log_info("WiaItem->Download(): Done");
		add_msg(
			self, WIA_MSG_END_OF_FEED,
			NULL /* data */,  0 /* length */
		);
		goto end;
	} else if (download_hr == WIA_ERROR_PAPER_EMPTY) {
		lis_log_info("WiaItem->Download(): No more paper");
		add_msg(
			self, WIA_MSG_END_OF_FEED,
			NULL /* data */,  0 /* length */
		);
		goto end;
 	} else {
		err = hresult_to_lis_error(download_hr);
		lis_log_error(
			"WiaItem->Download() failed: 0x%lX, 0x%X, %s",
			download_hr, err, lis_strerror(err)
		);
		add_error(self, err);
		goto end;
	}

end:
	lis_log_info("End of scan thread");
	if (wia_transfer != NULL) {
		lis_log_debug("WiaTransfer->Release() ...");
		wia_transfer->lpVtbl->Release(wia_transfer);
		lis_log_debug("WiaTransfer->Release() done");
	}
	return 0;
}


enum lis_error wia_transfer_new(
		LisIWiaItem2 *in_wia_item,
		IWiaPropertyStorage *in_wia_props,
		struct wia_transfer **out_transfer
	)
{
	struct wia_transfer *self = NULL;
	enum lis_error err;

	self = GlobalAlloc(GPTR, sizeof(struct wia_transfer));
	if (self == NULL) {
		lis_log_error("Out of memory");
		err = LIS_ERR_NO_MEM;
		goto err;
	}

	// The structure wia_transfer actually contains 3 objects:
	// IStream, IWiaAppErrorHandler and IWiaTransferCallback
	self->refcount = 3;

	self->wia_item = in_wia_item;
	self->wia_item->lpVtbl->AddRef(self->wia_item);
	self->wia_props = in_wia_props;
	self->wia_props->lpVtbl->AddRef(self->wia_props);

	memcpy(
		&self->scan_session, &g_scan_session_template,
		sizeof(self->scan_session)
	);

	self->app_error_handler.lpVtbl = &g_wia_app_error_handler;
	self->istream.lpVtbl = &g_wia_stream;
	self->transfer_callback.lpVtbl = &g_wia_transfer_callback;

	InitializeConditionVariable(&self->scan.condition_variable);
	InitializeCriticalSection(&self->scan.critical_section);

	self->scan.thread = CreateThread(
		NULL, // lpThreadAttributes (non-inheritable)
		0, // dwStackSize (default)
		thread_download,
		self,
		0, // dwCreationFlags (immediate start)
		NULL // lpThreadId (ignored)
	);
	if (self->scan.thread == NULL) {
		lis_log_error(
			"CreateThread(Download) failed: %ld", GetLastError()
		);
		err = LIS_ERR_INTERNAL_UNKNOWN_ERROR;
		goto err;
	}

	*out_transfer = self;
	return LIS_OK;

err:
	if (self != NULL) {
		self->wia_item->lpVtbl->Release(self->wia_item);
		self->wia_props->lpVtbl->Release(self->wia_props);
		GlobalFree(self);
	}
	return err;
}


struct lis_scan_session *wia_transfer_get_scan_session(
		struct wia_transfer *self
	)
{
	return &self->scan_session;
}


void wia_transfer_free(struct wia_transfer *self)
{
	scan_cancel(&self->scan_session);
}
