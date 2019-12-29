#ifndef __LIBINSANE_ENDIANESS_H
#define __LIBINSANE_ENDIANESS_H

#ifdef OS_LINUX
#include <endian.h>
#else

// XXX(Jflesch): assuming Windows x86 --> little endian

#define le16toh(v) (v)
#define le32toh(v) (v)
#define htole32(v) (v)
#define htole16(v) (v)

static inline uint16_t be16toh(uint16_t v)
{
	return ((v << 8) | (v >> 8));
}

static inline uint16_t htobe16(uint16_t v)
{
	return ((v << 8) | (v >> 8));
}

#endif

#endif
