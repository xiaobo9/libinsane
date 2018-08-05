#ifndef __LIBINSANE_TESTS_UTIL_H
#define __LIBINSANE_TESTS_UTIL_H

/* macros to satisfy static analysis when using Cunit:
 * Static analysis doesn't seem to get that CU_ASSERT_xxx() do stop the test
 * when they fails.
 */

#define LIS_ASSERT_TRUE(x) do { \
		int _r = (x); \
		CU_ASSERT_TRUE(_r); \
		if (!(_r)) return; \
	} while (0)

#define LIS_ASSERT_FALSE(x) do { \
		int _r = (x); \
		CU_ASSERT_FALSE(_r); \
		if (_r) return; \
	} while (0)

#define LIS_ASSERT_EQUAL(x, y) do { \
		int _r = ((x) == (y)); \
		CU_ASSERT_TRUE(_r); \
		if (!_r) return; \
	} while (0)

#define LIS_ASSERT_NOT_EQUAL(x, y) do { \
		int _r = ((x) == (y)); \
		CU_ASSERT_FALSE(_r); \
		if (_r) return; \
	} while (0)

#endif
