#ifndef __LIBINSANE_BASES_WIA_PROPERTIES_H
#define __LIBINSANE_BASES_WIA_PROPERTIES_H

#include <windows.h>
#include <wia.h>
#include <sti.h>

#include <libinsane/capi.h>


struct lis_wia2lis_possibles {
	int eol;
	union {
		int integer;
		const char *str;
		const CLSID *clsid;
	} wia;
	union lis_value lis;
};


struct lis_wia2lis_property {
	int line; // useful to find back the property quickly

	enum {
		LIS_PROPERTY_DEVICE,
		LIS_PROPERTY_ITEM,
	} item_type;

	struct {
		PROPID id;
		VARTYPE type;
	} wia;

	struct {
		const char *name;
		enum lis_value_type type;
	} lis;

	const struct lis_wia2lis_possibles *possibles;
};


/* for tests only */
const struct lis_wia2lis_property *lis_get_all_properties(
	size_t *nb_properties
);

#endif
