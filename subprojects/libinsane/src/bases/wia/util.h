#ifndef __LIBINSANE_WIA_UTIL_H
#define __LIBINSANE_WIA_UTIL_H

#include <windows.h>

BSTR lis_cstr2bstr(const char *s);
char *lis_bstr2cstr(BSTR bstr);
char *lis_propvariant2char(PROPVARIANT *prop);

#endif
