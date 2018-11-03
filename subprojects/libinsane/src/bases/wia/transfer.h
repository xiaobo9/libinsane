#ifndef __LIBINSANE_WIA_TRANSFER_H
#define __LIBINSANE_WIA_TRANSFER_H

#include <objidl.h>
#include <wia.h>
#include <windows.h>

#include <libinsane/capi.h>
#include <libinsane/error.h>

#include "wia2.h"

struct wia_transfer;


enum lis_error wia_transfer_new(
	LisIWiaItem2 *in_wia_item,
	IWiaPropertyStorage *in_wia_props,
	struct wia_transfer **out_transfer
);
struct lis_scan_session *wia_transfer_get_scan_session(
	struct wia_transfer *self
);
void wia_transfer_free(struct wia_transfer *self);

#endif
