#ifndef __LIBINSANE_BASES_WIA_PROPERTIES_H
#define __LIBINSANE_BASES_WIA_PROPERTIES_H

#include <stdbool.h>

#include <windows.h>
#include <wia.h>
#include <sti.h>

#include <libinsane/capi.h>

#define LIS_MAX_NB_PROPERTIES 256


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

	enum wia2lis_item_type {
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


const struct lis_wia2lis_property *lis_wia2lis_get_property(
	bool root, PROPID propid
);


enum lis_error lis_wia2lis_get_possibles(
	const struct lis_wia2lis_property *in_wia2lis,
	struct lis_value_list *out_list
);

enum lis_error lis_wia2lis_get_range(
	const struct lis_wia2lis_property *in_wia2lis,
	PROPVARIANT in_propvariants,
	struct lis_value_range *out_range
);

enum lis_error lis_wia2lis_get_list(
	const struct lis_wia2lis_property *in_wia2lis,
	PROPVARIANT in_propvariants,
	struct lis_value_list *out_list
);


enum lis_error lis_convert_wia2lis(
	const struct lis_wia2lis_property *wia2lis,
	const PROPVARIANT *propvariant,
	union lis_value *value,
	char **allocated
);


enum lis_error lis_convert_lis2wia(
	const struct lis_wia2lis_property *wia2lis,
	union lis_value in_value,
	PROPVARIANT *out_propvariant
);


/* for tests only */
const struct lis_wia2lis_property *lis_get_all_properties(
	size_t *nb_properties
);

#endif
