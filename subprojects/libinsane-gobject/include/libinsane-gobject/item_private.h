#ifndef __LIBINSANE_ITEM_PRIVATE_H
#define __LIBINSANE_ITEM_PRIVATE_H

#include <libinsane/capi.h>

#include "item.h"

LibinsaneItem *libinsane_item_new_from_libinsane(
	GObject *parent, gboolean root, struct lis_item *lis_item
);

#endif
