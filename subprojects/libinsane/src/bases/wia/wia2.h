#ifndef __LIBINSANE_BASES_WIA2_H
#define __LIBINSANE_BASES_WIA2_H

#include <wia.h>
#include <windows.h>
#include <unknwn.h>


// Microsoft WIA2 structures.
// It seems at the moment they are not available yet in Mingw64.


struct LisIWiaItem2 {
	CONST_VTBL struct LisIWiaItem2Vtbl *lpVtbl;
};


typedef struct LisIEnumWiaItem2 {
	CONST_VTBL struct LisIWiaItem2Vtbl *lpVtbl;
} LisIEnumWiaItem2;


typedef struct LisIEnumWiaItem2Vtbl {
	BEGIN_INTERFACE

	IUnknownVtbl unknown;

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

	IUnknownVtbl unknown;

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


DEFINE_GUID(
	CLSID_WiaDevMgr2,
	0xB6CE92BC,
	0x7C88,
	0x41EE,
	0x8B, 0x54,
	0xBE, 0xC9, 0x26, 0x17, 0xE5, 0x99
);


typedef struct LisIWiaDevMgr2 {
	CONST_VTBL struct LisIWiaDevMgr2Vtbl *lpVtbl;
} LisIWiaDevMgr2;


typedef struct LisIWiaDevMgr2Vtbl {
	BEGIN_INTERFACE

	IUnknownVtbl unknown;

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


#endif
