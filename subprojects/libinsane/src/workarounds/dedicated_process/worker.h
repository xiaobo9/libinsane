#ifndef __LIBINSANE_WORKAROUND_DEDICATED_PROCESS_WORKER_H
#define __LIBINSANE_WORKAROUND_DEDICATED_PROCESS_WORKER_H

#include <libinsane/capi.h>

#include "protocol.h"

void lis_worker_main(struct lis_api *to_wrap, struct lis_pipes *pipes);

#endif
