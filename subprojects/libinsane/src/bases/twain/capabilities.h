#ifndef __LIBINSANE_TWAIN_CAPABILITIES_H
#define __LIBINSANE_TWAIN_CAPABILITIES_H

#include <stdbool.h>
#include <windows.h>

#include "twain.h"


struct lis_twain_cap_possible {
	int eol;
	const char *str;
	int twain_int;
};

struct lis_twain_cap {
	const char *name;
	const char *desc;

	bool readonly;

	TW_UINT16 id;
	TW_UINT16 type;

	struct lis_twain_cap_possible *possibles;
};

const struct lis_twain_cap *lis_twain_get_all_caps(int *nb_caps);

#endif
