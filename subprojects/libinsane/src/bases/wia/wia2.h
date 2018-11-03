#ifndef __LIBINSANE_BASES_WIA2_H
#define __LIBINSANE_BASES_WIA2_H

#include <wia.h>
#include <windows.h>
#include <unknwn.h>


// Microsoft WIA2 structures / defines.
// It seems at the moment they are not available yet in Mingw64.


#define LIS_WIA_DIP_PNP_ID 16
#define LIS_WIA_DIP_STI_DRIVER_VERSION 17
#define LIS_WIA_DPS_DEVICE_ID 3114
#define LIS_WIA_DPS_GLOBAL_IDENTITY 3115
#define LIS_WIA_DPS_SCAN_AVAILABLE_ITEM 3116
#define LIS_WIA_DPS_SERVICE_ID 3113
#define LIS_WIA_DPS_USER_NAME 3112
#define LIS_WIA_IPA_ITEM_CATEGORY 4125
#define LIS_WIA_IPA_ITEMS_STORED 4127
#define LIS_WIA_IPA_ITEMS_STORED 4127
#define LIS_WIA_IPA_UPLOAD_ITEM_SIZE 4126
#define LIS_WIA_IPA_UPLOAD_ITEM_SIZE 4126
#define LIS_WIA_IPS_AUTO_DESKEW 3107
#define LIS_WIA_IPS_DESKEW_X 6162
#define LIS_WIA_IPS_DESKEW_Y 6163
#define LIS_WIA_IPS_DOCUMENT_HANDLING_SELECT 3088
#define LIS_WIA_IPS_FILM_NODE_NAME 4129
#define LIS_WIA_IPS_FILM_SCAN_MODE 3104
#define LIS_WIA_IPS_LAMP 3105
#define LIS_WIA_IPS_LAMP_AUTO_OFF 3106
#define LIS_WIA_IPS_MAX_HORIZONTAL_SIZE 6165
#define LIS_WIA_IPS_MAX_VERTICAL_SIZE 6166
#define LIS_WIA_IPS_MIN_HORIZONTAL_SIZE 6167
#define LIS_WIA_IPS_MIN_VERTICAL_SIZE 6168
#define LIS_WIA_IPS_OPTICAL_XRES 3090
#define LIS_WIA_IPS_OPTICAL_YRES 3091
#define LIS_WIA_IPS_PAGE_HEIGHT 3099
#define LIS_WIA_IPS_PAGE_SIZE 3097
#define LIS_WIA_IPS_PAGE_WIDTH 3098
#define LIS_WIA_IPS_PAGES 3096
#define LIS_WIA_IPS_PREVIEW 3100
#define LIS_WIA_IPS_PREVIEW_TYPE 3111
#define LIS_WIA_IPS_SEGMENTATION 6164
#define LIS_WIA_IPS_SHEET_FEEDER_REGISTRATION 3078
#define LIS_WIA_IPS_SHOW_PREVIEW_CONTROL 3103
#define LIS_WIA_IPS_SUPPORTS_CHILD_ITEM_CREATION 3108
#define LIS_WIA_IPS_TRANSFER_CAPABILITIES 6169
#define LIS_WIA_IPS_XSCALING 3109
#define LIS_WIA_IPS_YSCALING 3110


DEFINE_GUID(
	LIS_WIA_CATEGORY_FINISHED_FILE,
	0xff2b77ca, 0xcf84, 0x432b,
	0xa7, 0x35, 0x3a, 0x13, 0x0d, 0xde, 0x2a, 0x88
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FLATBED,
	0xfb607b1f, 0x43f3, 0x488b,
	0x85, 0x5b, 0xfb, 0x70, 0x3e, 0xc3, 0x42, 0xa6
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FEEDER,
	0xfe131934, 0xf84c, 0x42ad,
	0x8d, 0xa4, 0x61, 0x29, 0xcd, 0xdd, 0x72, 0x88
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FILM,
	0xfcf65be7, 0x3ce3, 0x4473,
	0xaf, 0x85, 0xf5, 0xd3, 0x7d, 0x21, 0xb6, 0x8a
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_ROOT,
	0xf193526f, 0x59b8, 0x4a26,
	0x98, 0x88, 0xe1, 0x6e, 0x4f, 0x97, 0xce, 0x10
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FOLDER,
	0xc692a446, 0x6f5a, 0x481d,
	0x85, 0xbb, 0x92, 0xe2, 0xe8, 0x6f, 0xd3, 0xa
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FEEDER_FRONT,
	0x4823175c, 0x3b28, 0x487b,
	0xa7, 0xe6, 0xee, 0xbc, 0x17, 0x61, 0x4f, 0xd1
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_FEEDER_BACK,
	0x61ca74d4, 0x39db, 0x42aa,
	0x89, 0xb1, 0x8c, 0x19, 0xc9, 0xcd, 0x4c, 0x23
);
DEFINE_GUID(
	LIS_WIA_CATEGORY_AUTO,
	0xdefe5fd8, 0x6c97, 0x4dde,
	0xb1, 0x1e, 0xcb, 0x50, 0x9b, 0x27, 0x0e, 0x11
);


#define LIS_WIA_COMPRESSION_AUTO 100
#define LIS_WIA_COMPRESSION_JBIG 6
#define LIS_WIA_COMPRESSION_JPEG2K 7
#define LIS_WIA_COMPRESSION_PNG 8


#define LIS_WIA_DATA_AUTO 100
#define LIS_WIA_DATA_RAW_RGB 6
#define LIS_WIA_DATA_RAW_BGR 7
#define LIS_WIA_DATA_RAW_YUV 8
#define LIS_WIA_DATA_RAW_YUVK 9
#define LIS_WIA_DATA_RAW_CMY 10
#define LIS_WIA_DATA_RAW_CMYK 11


#define LIS_FILM_TPA_READY 0x40
#define LIS_STORAGE_READY 0x80
#define LIS_STORAGE_FULL 0x100
#define LIS_MULTIPLE_FEED 0x200
#define LIS_DEVICE_ATTENTION 0x400
#define LIS_LAMP_ERR 0x800


#define LIS_ADVANCED_DUP 0x2000
#define LIS_AUTO_SOURCE 0x8000
#define LIS_DETECT_FILM_TPA 0x400
#define LIS_FILM_TPA 0x200
#define LIS_STOR 0x800


#define LIS_WIA_ADVANCED_PREVIEW 0
#define LIS_WIA_BASIC_PREVIEW 1


#define LIS_WIA_AUTO_DESKEW_ON 0
#define LIS_WIA_AUTO_DESKEW_OFF 1


#define LIS_WIA_FILM_COLOR_SLIDE 0
#define LIS_WIA_FILM_COLOR_NEGATIVE 1
#define LIS_WIA_FILM_BW_NEGATIVE 2


#define LIS_WIA_PAGE_USLEGAL 3
#define LIS_WIA_PAGE_USLETTER WIA_PAGE_LETTER
#define LIS_WIA_PAGE_USLEDGER 4
#define LIS_WIA_PAGE_USSTATEMENT 5
#define LIS_WIA_PAGE_BUSINESSCARD 6
#define LIS_WIA_PAGE_ISO_A0 7
#define LIS_WIA_PAGE_ISO_A1 8
#define LIS_WIA_PAGE_ISO_A2 9
#define LIS_WIA_PAGE_ISO_A3 10
#define LIS_WIA_PAGE_ISO_A4 WIA_PAGE_A4
#define LIS_WIA_PAGE_ISO_A5 11
#define LIS_WIA_PAGE_ISO_A6 12
#define LIS_WIA_PAGE_ISO_A7 13
#define LIS_WIA_PAGE_ISO_A8 14
#define LIS_WIA_PAGE_ISO_A9 15
#define LIS_WIA_PAGE_ISO_A10 16
#define LIS_WIA_PAGE_ISO_B0 17
#define LIS_WIA_PAGE_ISO_B1 18
#define LIS_WIA_PAGE_ISO_B2 19
#define LIS_WIA_PAGE_ISO_B3 20
#define LIS_WIA_PAGE_ISO_B4 21
#define LIS_WIA_PAGE_ISO_B5 22
#define LIS_WIA_PAGE_ISO_B6 23
#define LIS_WIA_PAGE_ISO_B7 24
#define LIS_WIA_PAGE_ISO_B8 25
#define LIS_WIA_PAGE_ISO_B9 26
#define LIS_WIA_PAGE_ISO_B10 27
#define LIS_WIA_PAGE_ISO_C0 28
#define LIS_WIA_PAGE_ISO_C1 29
#define LIS_WIA_PAGE_ISO_C2 30
#define LIS_WIA_PAGE_ISO_C3 31
#define LIS_WIA_PAGE_ISO_C4 32
#define LIS_WIA_PAGE_ISO_C5 33
#define LIS_WIA_PAGE_ISO_C6 34
#define LIS_WIA_PAGE_ISO_C7 35
#define LIS_WIA_PAGE_ISO_C8 36
#define LIS_WIA_PAGE_ISO_C9 37
#define LIS_WIA_PAGE_ISO_C10 38
#define LIS_WIA_PAGE_JIS_B0 39
#define LIS_WIA_PAGE_JIS_B1 40
#define LIS_WIA_PAGE_JIS_B2 41
#define LIS_WIA_PAGE_JIS_B3 42
#define LIS_WIA_PAGE_JIS_B4 43
#define LIS_WIA_PAGE_JIS_B5 44
#define LIS_WIA_PAGE_JIS_B6 45
#define LIS_WIA_PAGE_JIS_B7 46
#define LIS_WIA_PAGE_JIS_B8 47
#define LIS_WIA_PAGE_JIS_B9 48
#define LIS_WIA_PAGE_JIS_B10 49
#define LIS_WIA_PAGE_JIS_2A 50
#define LIS_WIA_PAGE_JIS_4A 51
#define LIS_WIA_PAGE_DIN_2B 52
#define LIS_WIA_PAGE_DIN_4B 53
#define LIS_WIA_PAGE_AUTO 100
#define LIS_WIA_PAGE_CUSTOM_BASE 0x8000


#define LIS_WIA_LAMP_ON 0
#define LIS_WIA_LAMP_OFF 1


#define LIS_WIA_USE_SEGMENTATION_FILTER 0
#define LIS_WIA_DONT_USE_SEGMENTATION_FILTER 1


struct LisIWiaItem2 {
	CONST_VTBL struct LisIWiaItem2Vtbl *lpVtbl;
};


typedef struct LisIEnumWiaItem2 {
	CONST_VTBL struct LisIEnumWiaItem2Vtbl *lpVtbl;
} LisIEnumWiaItem2;


typedef struct LisIEnumWiaItem2Vtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIEnumWiaItem2 *self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(
		LisIEnumWiaItem2 *self
	);

	ULONG (WINAPI *Release)(
		LisIEnumWiaItem2 *self
	);

	HRESULT (WINAPI *Next)(
		LisIEnumWiaItem2 *self,
		ULONG cElt,
		struct LisIWiaItem2 **ppIWiaItem2,
		ULONG *pcEltFetched
	);

	HRESULT (WINAPI *Skip)(
		LisIEnumWiaItem2 *self,
		ULONG cElt
	);

	HRESULT (WINAPI *Reset)(
		LisIEnumWiaItem2 *self
	);

	HRESULT (WINAPI *Clone)(
		LisIEnumWiaItem2 *self,
		LisIEnumWiaItem2 **ppIEnum
	);

	HRESULT (WINAPI *GetCount)(
		LisIEnumWiaItem2 *self,
		ULONG *cElt
	);

	END_INTERFACE
} LisIEnumWiaItem2Vtbl;


typedef struct LisIWiaItem2 LisIWiaItem2;


typedef struct LisIWiaItem2Vtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIWiaItem2* self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(LisIWiaItem2* self);

	ULONG (WINAPI *Release)(LisIWiaItem2* self);

	HRESULT (WINAPI *CreateChildItem)(
		LisIWiaItem2 *self,
		LONG lItemFlags,
		LONG lCreationFlags,
		BSTR bstrItemName,
		LisIWiaItem2 **ppLisIWiaItem2
	);

	HRESULT (WINAPI *DeleteItem)(
		LisIWiaItem2 *self,
		LONG lFlags
	);

	HRESULT (WINAPI *EnumChildItems)(
		LisIWiaItem2 *self,
		const GUID *pCategoryGUID,
		LisIEnumWiaItem2 **ppIEnumWiaItem2
	);

	HRESULT (WINAPI *FindItemByName)(
		LisIWiaItem2 *self,
		LONG lFlags,
		BSTR bstrFullItemName,
		LisIWiaItem2 **ppLisIWiaItem2
	);

	HRESULT (WINAPI *GetItemCategory)(
		LisIWiaItem2 *self,
		GUID *pItemCategoryGUID
	);

	HRESULT (WINAPI *GetItemType)(
		LisIWiaItem2 *self,
		LONG *pItemType
	);

	HRESULT (WINAPI *DeviceDlg)(
		LisIWiaItem2 *self,
		LONG lFlags,
		HWND hwndParent,
		BSTR bstrFolderName,
		BSTR bstrFilename,
		LONG *plNumFiles,
		__deref_out_ecount(*plNumFiles)  BSTR **ppbstrFilePaths,
		LisIWiaItem2 **ppItem
	);

	HRESULT (WINAPI *DeviceCommand)(
		LisIWiaItem2 *self,
		LONG lFlags,
		const GUID *pCmdGUID,
		LisIWiaItem2 **ppLisIWiaItem2
	);

	HRESULT (WINAPI *EnumDeviceCapabilities)(
		LisIWiaItem2 *self,
		LONG lFlags,
		IEnumWIA_DEV_CAPS **ppIEnumWIA_DEV_CAPS
	);

	HRESULT (WINAPI *CheckExtension)(
		LisIWiaItem2 *self,
		LONG lFlags,
		BSTR bstrName,
		REFIID riidExtensionInterface,
		BOOL *pbExtensionExists
	);

	HRESULT (WINAPI *GetExtension)(
		LisIWiaItem2 *self,
		LONG lFlags,
		BSTR bstrName,
		REFIID riidExtensionInterface,
		void **ppOut
	);

	HRESULT (WINAPI *GetParentItem)(
		LisIWiaItem2 *self,
		LisIWiaItem2 **ppLisIWiaItem2
	);

	HRESULT (WINAPI *GetRootItem)(
		LisIWiaItem2 *self,
		LisIWiaItem2 **ppLisIWiaItem2
	);

	HRESULT (WINAPI *GetPreviewComponent)(
		LisIWiaItem2 *self,
		LONG lFlags,
		// Seriously, nobody cares about preview anymore.
		// IWiaPreview **ppWiaPreview
		void **nobodyCares
	);

	HRESULT (WINAPI *EnumRegisterEventInfo)(
		LisIWiaItem2 *self,
		LONG lFlags,
		const GUID *pEventGUID,
		IEnumWIA_DEV_CAPS **ppIEnum
	);

	HRESULT (WINAPI *Diagnostic)(
		LisIWiaItem2 *self,
		ULONG ulSize,
		__RPC__in_ecount_full(ulSize) BYTE *pBuffer
	);

	END_INTERFACE
} LisIWiaItem2Vtbl;



typedef struct LisIWiaDevMgr2 {
	CONST_VTBL struct LisIWiaDevMgr2Vtbl *lpVtbl;
} LisIWiaDevMgr2;


typedef struct LisIWiaDevMgr2Vtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIWiaDevMgr2* self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(LisIWiaDevMgr2* self);

	ULONG (WINAPI *Release)(LisIWiaDevMgr2* self);

	HRESULT (WINAPI *EnumDeviceInfo)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		IEnumWIA_DEV_INFO **ppIEnum
	);

	HRESULT (WINAPI *CreateDevice)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		BSTR bstrDeviceID,
		LisIWiaItem2 **ppWiaItem2Root
	);

	HRESULT (WINAPI *SelectDeviceDlg)(
		LisIWiaDevMgr2 *self,
		HWND hwndParent,
		LONG lDeviceType,
		LONG lFlags,
		BSTR *pbstrDeviceID,
		LisIWiaItem2 **ppItemRoot
	);

	HRESULT (WINAPI *SelectDeviceDlgID)(
		LisIWiaDevMgr2 *self,
		HWND hwndParent,
		LONG lDeviceType,
		LONG lFlags,
		BSTR *pbstrDeviceID
	);

	HRESULT (WINAPI *RegisterEventCallbackInterface)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		BSTR bstrDeviceID,
		const GUID *pEventGUID,
		IWiaEventCallback *pIWiaEventCallback,
		IUnknown **pEventObject
	);

	HRESULT (WINAPI *RegisterEventCallbackProgram)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		BSTR bstrDeviceID,
		const GUID *pEventGUID,
		BSTR bstrFullAppName,
		BSTR bstrCommandLineArg,
		BSTR bstrName,
		BSTR bstrDescription,
		BSTR bstrIcon
	);

	HRESULT (WINAPI *RegisterEventCallbackCLSID)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		BSTR bstrDeviceID,
		const GUID *pEventGUID,
		const GUID *pClsID,
		BSTR bstrName,
		BSTR bstrDescription,
		BSTR bstrIcon
	);

	HRESULT (WINAPI *GetImageDlg)(
		LisIWiaDevMgr2 *self,
		LONG lFlags,
		BSTR bstrDeviceID,
		HWND hwndParent,
		BSTR bstrFolderName,
		BSTR bstrFilename,
		LONG *plNumFiles,
		BSTR **ppbstrFilePaths,
		LisIWiaItem2 **ppItem
	);

	END_INTERFACE
} LisIWiaDevMgr2Vtbl;


typedef struct LisWiaTransferParams
{
	LONG lMessage;
	LONG lPercentComplete;
	ULONG64 ulTransferredBytes;
	HRESULT hrErrorStatus;
} LisWiaTransferParams;


typedef struct LisIWiaTransferCallback {
	CONST_VTBL struct LisIWiaTransferCallbackVtbl *lpVtbl;
} LisIWiaTransferCallback;


typedef struct LisIWiaTransferCallbackVtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIWiaTransferCallback *self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(
		LisIWiaTransferCallback *self
	);

	ULONG (WINAPI *Release)(
		LisIWiaTransferCallback *self
	);

	HRESULT (WINAPI *TransferCallback)(
		LisIWiaTransferCallback *self,
		LONG lFlags,
		LisWiaTransferParams *pWiaTransferParams
	);

	HRESULT (WINAPI *GetNextStream)(
		LisIWiaTransferCallback *self,
		LONG lFlags,
		BSTR bstrItemName,
		BSTR bstrFullItemName,
		IStream **ppDestination
	);

	END_INTERFACE
} LisIWiaTransferCallbackVtbl;


typedef struct LisIWiaTransfer {
	CONST_VTBL struct LisIWiaTransferVtbl *lpVtbl;
} LisIWiaTransfer;


typedef struct LisIWiaTransferVtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIWiaTransfer *self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(
		LisIWiaTransfer *self
	);

	ULONG (WINAPI *Release)(
		LisIWiaTransfer *self
	);

	HRESULT (WINAPI *Download)(
		LisIWiaTransfer *self,
		LONG lFlags,
		LisIWiaTransferCallback *pIWiaTransferCallback
	);

	HRESULT (WINAPI *Upload)(
		LisIWiaTransfer * This,
		LONG lFlags,
		IStream *pSource,
		LisIWiaTransferCallback *pIWiaTransferCallback
	);

	HRESULT (WINAPI *Cancel)(
		LisIWiaTransfer *self
	);

	HRESULT (WINAPI *EnumWIA_FORMAT_INFO)(
		LisIWiaTransfer *self,
		IEnumWIA_FORMAT_INFO **ppEnum
	);

	END_INTERFACE
} LisIWiaTransferVtbl;


typedef struct LisIWiaAppErrorHandler {
	CONST_VTBL struct LisIWiaAppErrorHandlerVtbl *lpVtbl;
} LisIWiaAppErrorHandler;


typedef struct LisIWiaAppErrorHandlerVtbl {
	BEGIN_INTERFACE

	HRESULT (WINAPI *QueryInterface)(
		LisIWiaAppErrorHandler *self,
		REFIID riid,
		void **ppvObject
	);

	ULONG (WINAPI *AddRef)(
		LisIWiaAppErrorHandler *self
	);

	ULONG (WINAPI *Release)(
		LisIWiaAppErrorHandler *self
	);

	HRESULT (WINAPI *GetWindow)(
		LisIWiaAppErrorHandler * This,
		HWND *phwnd
	);

	HRESULT (STDMETHODCALLTYPE *ReportStatus)(
		LisIWiaAppErrorHandler * This,
		LONG lFlags,
		LisIWiaItem2 *pWiaItem2,
		HRESULT hrStatus,
		LONG lPercentComplete
	);

	END_INTERFACE
} LisIWiaAppErrorHandlerVtbl;


DEFINE_GUID(
	IID_LisWiaAppErrorHandler,
	0x6C16186C,
	0xD0A6,
	0x400C,
	0x80, 0xF4,
	0xD2, 0x69, 0x86, 0xA0, 0xE7, 0x34
);


DEFINE_GUID(
	CLSID_LisWiaDevMgr2,
	0xB6C292BC,
	0x7C88,
	0x41EE,
	0x8B, 0x54,
	0x8E, 0xC9, 0x26, 0x17, 0xE5, 0x99
);

DEFINE_GUID(
	IID_LisIWiaDevMgr2,
	0x79C07CF1,
	0xCBDD,
	0x41EE,
	0x8E, 0xC3,
	0xF0, 0x00, 0x80, 0xCA, 0xDA, 0x7A
);

DEFINE_GUID(
	IID_LisWiaTransfer,
	0xc39d6942,
	0x2f4e,
	0x4d04,
	0x92, 0xfe,
	0x4e, 0xf4, 0xd3, 0xa1, 0xde, 0x5a
);

DEFINE_GUID(
	IID_LisWiaTransferCallback,
	0x27d4eaaf,
	0x28a6,
	0x4ca5,
	0x9a, 0xab,
	0xe6, 0x78, 0x16, 0x8b, 0x95, 0x27
);


#define LIS_WIA_TRANSFER_MSG_STATUS 1
#define LIS_WIA_TRANSFER_MSG_END_OF_STREAM 2
#define LIS_WIA_TRANSFER_MSG_END_OF_TRANSFER 3
#define LIS_WIA_TRANSFER_MSG_DEVICE_STATUS 5
#define LIS_WIA_TRANSFER_MSG_NEW_PAGE 6

#endif
