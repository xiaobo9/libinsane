#include <assert.h>

#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>


enum lis_error lis_api_workaround_dedicated_process(
		struct lis_api *to_wrap, struct lis_api **out_impl
	)
{
	// TODO
	*out_impl = to_wrap;
	return LIS_OK;
}
