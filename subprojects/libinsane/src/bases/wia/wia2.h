#ifndef __LIBINSANE_BASES_WIA2_H
#define __LIBINSANE_BASES_WIA2_H

#include <wia.h>
#include <windows.h>
#include <unknwn.h>


// Microsoft WIA2 structures / defines.
// It seems at the moment they are not available yet in Mingw64.

#define LIS_WIA_IPA_ITEM_CATEGORY 4125
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

#endif
