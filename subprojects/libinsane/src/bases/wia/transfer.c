#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libinsane/capi.h>
#include <libinsane/log.h>
#include <libinsane/util.h>

#include "transfer.h"
#include "util.h"


#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) \
      ((type *) ((char *)(ptr) - offsetof(type, member)))


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

	struct lis_scan_session scan_session;

	LisIWiaTransferCallback transfer_callback;
	IStream istream;

	struct {
		HANDLE thread;
		long written;

		CRITICAL_SECTION critical_section;
		CONDITION_VARIABLE condition_variable;

		struct wia_msg *first;
		struct wia_msg *last;
	} scan;
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


static enum lis_error get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *parameters
	)
{
	LIS_UNUSED(self);
	LIS_UNUSED(parameters);
	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static int end_of_feed(struct lis_scan_session *self)
{
	LIS_UNUSED(self);
	// TODO
	return 1;
}


static int end_of_page(struct lis_scan_session *self)
{
	LIS_UNUSED(self);
	// TODO
	return 1;
}


static enum lis_error scan_read(
		struct lis_scan_session *self, void *out_buffer,
		size_t *buffer_size
	)
{
	LIS_UNUSED(self);
	LIS_UNUSED(out_buffer);
	LIS_UNUSED(buffer_size);
	// TODO
	return LIS_ERR_INTERNAL_NOT_IMPLEMENTED;
}


static void pop_all_msg(struct wia_transfer *self);


static void scan_cancel(struct lis_scan_session *_self)
{
	struct wia_transfer *self = container_of(
		_self, struct wia_transfer, scan_session
	);

	if (self->scan.thread != NULL) {
		WaitForSingleObject(self->scan.thread, INFINITE);
		CloseHandle(self->scan.thread);
		pop_all_msg(self);
		self->scan.thread = NULL;
	}
}


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
	if (msg_length > 0) {
		memcpy(msg->data, data, msg_length);
	}

	EnterCriticalSection(&self->scan.critical_section);
	if (self->scan.last != NULL) {
		self->scan.last->next = msg;
	} else {
		self->scan.first = msg;
		self->scan.last = msg;
	}
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
		self->scan.last->next = msg;
	} else {
		self->scan.first = msg;
		self->scan.last = msg;
	}
	LeaveCriticalSection(&self->scan.critical_section);

	WakeConditionVariable(&self->scan.condition_variable);

	return S_OK;
}


static struct wia_msg *pop_msg(struct wia_transfer *self, bool wait)
{
	struct wia_msg *msg;

	EnterCriticalSection(&self->scan.critical_section);

	if (self->scan.first == NULL) {
		if (wait) {
			SleepConditionVariableCS(
				&self->scan.condition_variable,
				&self->scan.critical_section,
				INFINITE
			);
		} else {
			return NULL;
		}
	}

	msg = self->scan.first;

	if (msg->next == NULL) {
		self->scan.first = NULL;
		self->scan.last = NULL;
	} else {
		self->scan.first = msg->next;
	}

	LeaveCriticalSection(&self->scan.critical_section);

	return msg;
}


static void free_msg(struct wia_msg *msg)
{
	GlobalFree(msg);
}


static void pop_all_msg(struct wia_transfer *self)
{
	struct wia_msg *msg;

	while ((msg = pop_msg(self, FALSE /* wait */)) != NULL) {
		free_msg(msg);
	}
}


static HRESULT WINAPI wia_transfer_cb_query_interface(
		LisIWiaTransferCallback *self, REFIID riid, void **ppvObject
	)
{
	HRESULT hr;
	LPOLESTR str;
	char *cstr;

	if (IsEqualIID(riid, &IID_IUnknown)) {
		lis_log_info("WiaTransferCallback->QueryInterface(IUnknown)");
		*ppvObject = self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaTransferCallback)) {
		lis_log_info(
			"WiaTransferCallback->QueryInterface("
			"IWiaTransferCallback)"
		);
		*ppvObject = self;
		return S_OK;
	} else if (IsEqualIID(riid, &IID_LisWiaAppErrorHandler)) {
		lis_log_info(
			"WiaTransferCallback->QueryInterface("
			"IWiaAppErrorHandler)"
		);
		// TODO
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


static ULONG WINAPI wia_transfer_cb_add_ref(LisIWiaTransferCallback *self)
{
	LIS_UNUSED(self);
	lis_log_info("WiaTransfer->AddRef()");
	return 0;
}


static ULONG WINAPI wia_transfer_cb_release(LisIWiaTransferCallback *self)
{
	LIS_UNUSED(self);
	lis_log_info("WiaTransfer->Release()");
	return 0;
}


static HRESULT WINAPI wia_transfer_cb_transfer_callback(
		LisIWiaTransferCallback *_self, LONG lFlags,
		LisWiaTransferParams *pWiaTransferParams
	)
{
	LIS_UNUSED(_self);
	struct wia_transfer *self = container_of(
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
		lis_log_info("IStream->QueryInterface(IStrean)");
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


static ULONG WINAPI wia_stream_add_ref(IStream *self)
{
	LIS_UNUSED(self);
	lis_log_info("IStream->AddRef()");
	return 0;
}


static ULONG WINAPI wia_stream_release(IStream *self)
{
	LIS_UNUSED(self);
	lis_log_info("IStream->Release()");
	return 0;
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

	// TODO
	return E_NOTIMPL;
}


static HRESULT WINAPI wia_stream_write(
		IStream *_self,
		const void *pv,
		ULONG cb,
		ULONG *pcbWritten
	)
{
	struct wia_transfer *self = container_of(
		_self, struct wia_transfer, istream
	);
	LIS_UNUSED(pv);

	lis_log_info("IStream->Write(%lu B)", cb);

	*pcbWritten = cb;
	self->scan.written += cb;

	return add_msg(self, WIA_MSG_DATA, pv, cb);
}


static HRESULT WINAPI wia_stream_seek(
		IStream *_self,
		LARGE_INTEGER dlibMove,
		DWORD dwOrigin,
		ULARGE_INTEGER *plibNewPosition
	)
{
	struct wia_transfer *self = container_of(
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

	lis_log_info(
		"IStream->Seek(%ld, %s, %lu) (written=%ld)",
		(long)dlibMove.QuadPart, origin,
		(long)plibNewPosition->QuadPart,
		self->scan.written
	);

	switch(dwOrigin) {
		case STREAM_SEEK_SET:
			if (dlibMove.QuadPart == 0 && self->scan.written == 0) {
				/* Epson WorkForce ES-300W */
				plibNewPosition->QuadPart = 0;
				return S_OK;
			}
			break;
		case STREAM_SEEK_END:
			if (dlibMove.QuadPart == 0) {
				plibNewPosition->QuadPart = self->scan.written = 0;
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
	struct wia_transfer *self = container_of(
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
	struct wia_transfer *self = container_of(
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
	download_hr = wia_transfer->lpVtbl->Download(
		wia_transfer,
		0, /* unused */
		&self->transfer_callback
	);
	lis_log_debug(
		"WiaTransfer->Download(): 0x%lX",
		download_hr
	);

	if (!FAILED(download_hr)) {
		lis_log_info("WiaItem->Download(): Done");
		// TODO
	} else if (download_hr == WIA_ERROR_PAPER_EMPTY) {
		lis_log_info("WiaItem->Download(): No more paper");
		// TODO
 	} else {
		err = hresult_to_lis_error(download_hr);
		lis_log_error(
			"WiaItem->Download() failed: 0x%lX, 0x%X, %s",
			download_hr, err, lis_strerror(err)
		);
		// TODO
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

	self->wia_item = in_wia_item;
	memcpy(
		&self->scan_session, &g_scan_session_template,
		sizeof(self->scan_session)
	);

	self->transfer_callback.lpVtbl = &g_wia_transfer_callback;
	self->istream.lpVtbl = &g_wia_stream;

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
	GlobalFree(self);
}
