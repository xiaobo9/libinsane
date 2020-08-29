#ifndef __LIBINSANE_DEDICATED_PROCESS_SERIALIZATION_H
#define __LIBINSANE_DEDICATED_PROCESS_SERIALIZATION_H

#include <stdlib.h>

#include <libinsane/error.h>

size_t lis_compute_packed_size(const char *format, ...);
void lis_pack(void **out_serialized, const char *format, ...);
void lis_unpack(const void **in_serialized, const char *format, ...);

#endif
