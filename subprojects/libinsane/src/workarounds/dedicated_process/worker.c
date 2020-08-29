#include <errno.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include <libinsane/log.h>
#include <libinsane/util.h>

#include "pack.h"
#include "worker.h"

// #define DISABLE_CRASH_HANDLER
// #define DISABLE_REDIRECT_LOGS
// #define DISABLE_REDIRECT_STDERR


/* when lis_worker_main() is called, we just forked and we are in the child
 * process. So there cannot be any other thread. Therefore using global
 * variables here is actually safe.
 */

static struct lis_api *g_wrapped;
static struct lis_pipes *g_pipes;


#ifndef DISABLE_REDIRECT_LOGS
static lis_log_callback worker_log_callback;
#endif


typedef enum lis_error (lis_execute)(struct lis_msg *msg_in, struct lis_msg *msg_out);


#ifndef DISABLE_CRASH_HANDLER
static const struct {
	int signal;
	const char *desc;
} g_crash_signals[] = {
	{ .signal = SIGSEGV, .desc = "SEGMENTATION FAULT" },
	{ .signal = SIGABRT, .desc = "ABORT" },
	{ .signal = SIGBUS, .desc = "BUS ERROR" },
};
#endif


#ifndef DISABLE_REDIRECT_LOGS
static const struct lis_log_callbacks g_log_callbacks = {
	.callbacks = {
		[LIS_LOG_LVL_DEBUG] = worker_log_callback,
		[LIS_LOG_LVL_INFO] = worker_log_callback,
		[LIS_LOG_LVL_WARNING] = worker_log_callback,
		[LIS_LOG_LVL_ERROR] = worker_log_callback,
	},
};
#endif


static lis_execute execute_cleanup;
static lis_execute execute_list_devices;
static lis_execute execute_get_device;
static lis_execute execute_item_get_children;
static lis_execute execute_item_get_options;
static lis_execute execute_item_scan_start;
static lis_execute execute_item_close;
static lis_execute execute_opt_get;
static lis_execute execute_opt_set;
static lis_execute execute_session_get_scan_parameters;
static lis_execute execute_session_end_of_feed;
static lis_execute execute_session_end_of_page;
static lis_execute execute_session_scan_read;
static lis_execute execute_session_cancel;


static const struct {
	const char *name;
	lis_execute *callback;
} g_callbacks[] = {
	[LIS_MSG_API_CLEANUP] = {
		.name = "cleanup", .callback = execute_cleanup,
	},
	[LIS_MSG_API_LIST_DEVICES] = {
		.name = "list_devices", .callback = execute_list_devices,
	},
	[LIS_MSG_API_GET_DEVICE] = {
		.name = "get_device", .callback = execute_get_device,
	},
	[LIS_MSG_ITEM_GET_CHILDREN] = {
		.name = "item_get_children", .callback = execute_item_get_children,
	},
	[LIS_MSG_ITEM_GET_OPTIONS] = {
		.name = "item_get_options", .callback = execute_item_get_options,
	},
	[LIS_MSG_ITEM_SCAN_START] = {
		.name = "item_scan_start", .callback = execute_item_scan_start,
	},
	[LIS_MSG_ITEM_CLOSE] = {
		.name = "item_close", .callback = execute_item_close,
	},
	[LIS_MSG_OPT_GET] = {
		.name = "opt_get", .callback = execute_opt_get,
	},
	[LIS_MSG_OPT_SET] = {
		.name = "opt_set", .callback = execute_opt_set,
	},
	[LIS_MSG_SESSION_GET_SCAN_PARAMETERS] = {
		.name = "session_get_scan_parameters", .callback = execute_session_get_scan_parameters,
	},
	[LIS_MSG_SESSION_END_OF_FEED] = {
		.name = "session_end_of_feed", .callback = execute_session_end_of_feed,
	},
	[LIS_MSG_SESSION_END_OF_PAGE] = {
		.name = "session_end_of_page", .callback = execute_session_end_of_page,
	},
	[LIS_MSG_SESSION_SCAN_READ] = {
		.name = "session_scan_read", .callback = execute_session_scan_read,
	},
	[LIS_MSG_SESSION_CANCEL] = {
		.name = "session_cancel", .callback = execute_session_cancel,
	},
};


#ifndef DISABLE_REDIRECT_LOGS
static void worker_log_callback(enum lis_log_level lvl, const char *msg)
{
	enum lis_error err;

	err = lis_protocol_log_write(g_pipes, lvl, msg);
	if (LIS_IS_ERROR(err)) {
		fprintf(
			stderr, "WARNING: Failed to log: %d, %s. Message was: %s\n",
			errno, strerror(errno), msg
		);
	}
}
#endif


#ifndef DISABLE_CRASH_HANDLER
static void crash_handler(int sig) {
	pid_t mypid;
	void *stack[16];
	size_t size;
	unsigned int i;

	mypid = getpid();

	for (i = 0 ; i < LIS_COUNT_OF(g_crash_signals); i++) {
		if (g_crash_signals[i].signal == sig) {
			fprintf(
				stderr, "======== PID %d - %s ========\n",
				(int)mypid, g_crash_signals[i].desc);
			break;
		}
	}
	if (i >= LIS_COUNT_OF(g_crash_signals)) {
		fprintf(
			stderr, "======== PID %d - GOT SIGNAL %d ========\n",
			(int)mypid, sig
		);
	}

	// get void*'s for all entries on the stack
	size = backtrace(stack, LIS_COUNT_OF(stack));

	// print out all the frames to stderr
	backtrace_symbols_fd(stack, size, STDERR_FILENO);

	if (kill(mypid, sig) < 0) {
		fprintf(stderr, "KILL FAILED\n");
		abort();
	}
}
#endif


static enum lis_error execute_cleanup(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	LIS_UNUSED(msg_in);
	LIS_UNUSED(msg_out);
	// Nothing to do
	return LIS_OK;
}


static enum lis_error execute_list_devices(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	enum lis_device_locations *locs;
	struct lis_device_descriptor **descs = NULL;
	int i;
	void *ptr;

	locs = msg_in->raw.iov_base;

	msg_out->header.err = g_wrapped->list_devices(g_wrapped, *locs, &descs);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = lis_compute_packed_size("i", 0);
	for (i = 0 ; descs[i] != NULL ; i++) {
		msg_out->raw.iov_len += lis_compute_packed_size(
			"ssss",
			descs[i]->dev_id,
			descs[i]->vendor,
			descs[i]->model,
			descs[i]->type
		);
	}

	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	ptr = msg_out->raw.iov_base;

	lis_pack(&ptr, "i", i);

	for (i = 0 ; descs[i] != NULL ; i++) {
		lis_pack(
			&ptr, "ssss",
			descs[i]->dev_id,
			descs[i]->vendor,
			descs[i]->model,
			descs[i]->type
		);
	}

	return LIS_OK;
}


static enum lis_error execute_get_device(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	struct lis_item *item = NULL;
	void *ptr;

	msg_out->header.err = g_wrapped->get_device(g_wrapped, msg_in->raw.iov_base, &item);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = lis_compute_packed_size(
		"sdp", item->name, item->type, item
	);
	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		item->close(item);
		return LIS_ERR_NO_MEM;
	}
	ptr = msg_out->raw.iov_base;

	lis_pack(&ptr, "sdp", item->name, item->type, item);

	return LIS_OK;
}


static enum lis_error execute_item_close(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	struct lis_item *item;
	const void *ptr = msg_in->raw.iov_base;

	LIS_UNUSED(msg_out);

	lis_unpack(&ptr, "p", &item);

	item->close(item);
	return LIS_OK;
}


static enum lis_error execute_item_get_children(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	struct lis_item *item;
	struct lis_item **children;
	int i;
	const void *ptr_in;
	void *ptr_out;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &item);

	msg_out->header.err = item->get_children(item, &children);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = lis_compute_packed_size("i", 0);
	for (i = 0 ; children[i] != NULL ; i++) {
		msg_out->raw.iov_len += lis_compute_packed_size(
			"sdp",
			children[i]->name,
			children[i]->type,
			children[i]
		);
	}

	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	ptr_out = msg_out->raw.iov_base;

	lis_pack(&ptr_out, "i", i);
	for (i = 0 ; children[i] != NULL ; i++) {
		lis_pack(
			&ptr_out, "sdp",
			children[i]->name,
			children[i]->type,
			children[i]
		);
	}

	return msg_out->header.err;
}


static void serialize_range(
	enum lis_value_type vtype, const struct lis_value_range *range,
	void **buf, size_t *buf_size)
{
	if (buf_size != NULL) {
		*buf_size += lis_compute_packed_size(
			"vvv",
			vtype, range->min,
			vtype, range->max,
			vtype, range->interval
		);
	}
	if (buf != NULL) {
		lis_pack(
			buf, "vvv",
			vtype, range->min,
			vtype, range->max,
			vtype, range->interval
		);
	}
}


static void serialize_list(
	enum lis_value_type vtype, const struct lis_value_list *list,
	void **buf, size_t *buf_size)
{
	int i;

	if (buf_size != NULL)
		*buf_size += lis_compute_packed_size("d", list->nb_values);
	if (buf != NULL)
		lis_pack(buf, "d", list->nb_values);

	for (i = 0 ; i < list->nb_values ; i++) {
		if (buf_size != NULL) {
			*buf_size += lis_compute_packed_size(
				"v", vtype, list->values[i]
			);
		}
		if (buf != NULL) {
			lis_pack(buf, "v", vtype, list->values[i]);
		}
	}
}


static void serialize_option(const struct lis_option_descriptor *desc, void **buf, size_t *buf_size)
{
	if (buf_size != NULL) {
		*buf_size += lis_compute_packed_size(
			"psssdddd",
			desc,
			desc->name,
			desc->title,
			desc->desc,
			desc->capabilities,
			desc->value.type,
			desc->value.unit,
			desc->constraint.type
		);
	}
	if (buf != NULL) {
		lis_pack(
			buf,
			"psssdddd",
			desc,
			desc->name,
			desc->title,
			desc->desc,
			desc->capabilities,
			desc->value.type,
			desc->value.unit,
			desc->constraint.type
		);
	}

	switch(desc->constraint.type) {
		case LIS_CONSTRAINT_NONE:
			break;
		case LIS_CONSTRAINT_RANGE:
			serialize_range(
				desc->value.type, &desc->constraint.possible.range,
				buf, buf_size
			);
			break;
		case LIS_CONSTRAINT_LIST:
			serialize_list(
				desc->value.type, &desc->constraint.possible.list,
				buf, buf_size
			);
			break;
	}
}


static void serialize_options(struct lis_option_descriptor **descs, void **buf, size_t *buf_size)
{
	int i;

	if (buf_size != NULL)
		*buf_size = 0;

	for (i = 0 ; descs[i] != NULL ; i++) { }
	if (buf_size != NULL)
		*buf_size += lis_compute_packed_size("d", i);
	if (buf != NULL)
		lis_pack(buf, "d", i);

	for (i = 0 ; descs[i] != NULL ; i++) {
		serialize_option(descs[i], buf, buf_size);
	}
}


static enum lis_error execute_item_get_options(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	void *ptr_out;
	struct lis_item *item;
	struct lis_option_descriptor **descs;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &item);

	msg_out->header.err = item->get_options(item, &descs);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	serialize_options(descs, NULL, &msg_out->raw.iov_len);

	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	ptr_out = msg_out->raw.iov_base;
	serialize_options(descs, &ptr_out, NULL);
	return LIS_OK;
}


static enum lis_error execute_opt_get(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	void *ptr_out;
	struct lis_option_descriptor *opt;
	union lis_value value;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &opt);

	msg_out->header.err = opt->fn.get_value(opt, &value);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = lis_compute_packed_size(
		"v", opt->value.type, value
	);
	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	ptr_out = msg_out->raw.iov_base;
	lis_pack(&ptr_out, "v", opt->value.type, value);
	return LIS_OK;
}


static enum lis_error execute_opt_set(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_option_descriptor *opt;
	union lis_value value;
	int set_flags = 0;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &opt);
	lis_unpack(&ptr_in, "v", opt->value.type, &value);

	msg_out->header.err = opt->fn.set_value(opt, value, &set_flags);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = sizeof(set_flags);
	msg_out->raw.iov_base = malloc(sizeof(set_flags));
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	memcpy(msg_out->raw.iov_base, &set_flags, sizeof(set_flags));
	return LIS_OK;
}


static enum lis_error execute_item_scan_start(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	void *ptr_out;
	struct lis_scan_session *session;
	struct lis_item *item;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &item);

	msg_out->header.err = item->scan_start(item, &session);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	msg_out->raw.iov_len = lis_compute_packed_size("p", session);
	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	ptr_out = msg_out->raw.iov_base;
	lis_pack(&ptr_out, "p", session);
	return LIS_OK;
}


static enum lis_error execute_session_get_scan_parameters(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_scan_session *session;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &session);

	msg_out->raw.iov_len = sizeof(struct lis_scan_parameters);
	msg_out->raw.iov_base = malloc(sizeof(struct lis_scan_parameters));
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	msg_out->header.err = session->get_scan_parameters(
		session, msg_out->raw.iov_base
	);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		return msg_out->header.err;
	}

	return msg_out->header.err;
}


static enum lis_error execute_session_end_of_feed(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_scan_session *session;
	void *ptr_out;
	int r;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &session);

	r = session->end_of_feed(session);

	msg_out->raw.iov_len = lis_compute_packed_size("d", r);
	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	ptr_out = msg_out->raw.iov_base;
	lis_pack(&ptr_out, "d", r);
	return LIS_OK;
}


static enum lis_error execute_session_end_of_page(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_scan_session *session;
	void *ptr_out;
	int r;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &session);

	r = session->end_of_page(session);

	msg_out->raw.iov_len = lis_compute_packed_size("d", r);
	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}
	ptr_out = msg_out->raw.iov_base;
	lis_pack(&ptr_out, "d", r);
	return LIS_OK;
}


static enum lis_error execute_session_scan_read(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_scan_session *session;

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "pd", &session, &msg_out->raw.iov_len);

	msg_out->raw.iov_base = malloc(msg_out->raw.iov_len);
	if (msg_out->raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	msg_out->header.err = session->scan_read(
		session, msg_out->raw.iov_base, &msg_out->raw.iov_len
	);
	if (LIS_IS_ERROR(msg_out->header.err)) {
		FREE(msg_out->raw.iov_base);
		return msg_out->header.err;
	}

	return msg_out->header.err;
}


static enum lis_error execute_session_cancel(struct lis_msg *msg_in, struct lis_msg *msg_out)
{
	const void *ptr_in;
	struct lis_scan_session *session;

	LIS_UNUSED(msg_out);

	ptr_in = msg_in->raw.iov_base;
	lis_unpack(&ptr_in, "p", &session);

	session->cancel(session);
	return LIS_OK;
}


static enum lis_error lis_worker_main_loop(void)
{
	enum lis_error err;
	struct lis_msg msg_in, msg_out;
	enum lis_msg_type msg_type;

	lis_log_info("Worker ready");

	do {

		memset(&msg_in, 0, sizeof(msg_in));
		memset(&msg_out, 0, sizeof(msg_out));

		err = lis_protocol_msg_read(
			g_pipes->sorted.msgs_m2w[0],
			&msg_in
		);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to read message: 0x%X, %s",
				err, lis_strerror(err)
			);
			break;
		}

		msg_out.header.msg_type = msg_in.header.msg_type;
		msg_type = msg_in.header.msg_type;
		msg_out.header.err = LIS_OK;

		lis_log_debug(
			"Processing %d '%s'",
			msg_in.header.msg_type,
			g_callbacks[msg_in.header.msg_type].name
		);
		err = g_callbacks[msg_in.header.msg_type].callback(&msg_in, &msg_out);
		if (LIS_IS_ERROR(err)) {
			msg_out.header.err = err;
		}

		err = lis_protocol_msg_write(
			g_pipes->sorted.msgs_w2m[1],
			&msg_out
		);
		lis_protocol_msg_free(&msg_in);
		lis_protocol_msg_free(&msg_out);
		if (LIS_IS_ERROR(err)) {
			lis_log_error(
				"Failed to write message: 0x%X, %s",
				err, lis_strerror(err)
			);
			break;
		}

	} while(msg_type != LIS_MSG_API_CLEANUP);

	return err;
}


void lis_worker_main(struct lis_api *to_wrap, struct lis_pipes *pipes)
{
	int fd_limit;
	int fd;
	unsigned int i, j;
	enum lis_error err;
#ifndef DISABLE_CRASH_HANDLER
	struct sigaction sig_act;
#endif

	g_wrapped = to_wrap;
	g_pipes = pipes;

#ifndef DISABLE_REDIRECT_LOGS
	/* We replace any log callback that calling application may have set:
	 * Logs needs to go through a pipe so the master process can then
	 * send them to the application
	 */
	lis_set_log_callbacks(&g_log_callbacks);
#else
	lis_log_reset();
#endif

#ifndef DISABLE_REDIRECT_STDERR
	if (dup2(pipes->sorted.stderr[1], STDOUT_FILENO) < 0
		|| dup2(pipes->sorted.stderr[1], STDERR_FILENO) < 0) {
		lis_log_warning(
			"Failed to redirect stderr and stdout: %d, %s", errno, strerror(errno)
		);
	}
#endif

#ifndef DISABLE_CRASH_HANDLER
	for (i = 0 ; i < LIS_COUNT_OF(g_crash_signals) ; i++) {
		lis_log_info("Adding handler for signal %d", g_crash_signals[i].signal);
		if (sigaction(g_crash_signals[i].signal, NULL, &sig_act) < 0) {
			lis_log_warning(
				"Failed to get current signal handler for %d: %d, %s",
				g_crash_signals[i].signal, errno, strerror(errno)
			);
		}
		sig_act.sa_handler = crash_handler;
		sig_act.sa_flags |= SA_RESETHAND;
		if (sigaction(g_crash_signals[i].signal, &sig_act, NULL) < 0) {
			lis_log_warning(
				"Failed to set signal handler for %d: %d, %s",
				g_crash_signals[i].signal, errno, strerror(errno)
			);
		}
	}
#endif

	// Close all the file descriptors but the pipes
	close(STDIN_FILENO);
	fd_limit = sysconf(_SC_OPEN_MAX);
	for (fd = STDERR_FILENO + 1; fd < fd_limit; fd++) {
		for (i = 0 ; i < LIS_COUNT_OF(pipes->all) ; i++) {
			for (j = 0 ; j < LIS_COUNT_OF(pipes->all[i]) ; j++) {
				if (pipes->all[i][j] == fd) {
					break;
				}
			}
			if (j < LIS_COUNT_OF(pipes->all[i])) {
				break;
			}
		}
		if (i < LIS_COUNT_OF(pipes->all)) {
			continue;
		}
		close(fd);
	}

	err = lis_worker_main_loop();

	exit(LIS_IS_OK(err) ? EXIT_SUCCESS : EXIT_FAILURE);
}
