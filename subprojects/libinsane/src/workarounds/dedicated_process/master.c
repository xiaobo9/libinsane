#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>

#include "pack.h"
#include "protocol.h"
#include "worker.h"


struct lis_master_impl
{
	struct lis_api parent;

	struct lis_api *wrapped;

	struct lis_pipes pipes;
	pid_t worker;
	pthread_t log_thread;

	struct {
		struct {
			void *msg;
			struct lis_device_descriptor **dev_ptrs;
			struct lis_device_descriptor *devs;
		} list_devs;
	} data;
};
#define LIS_MASTER_IMPL_PRIVATE(impl) ((struct lis_master_impl *)(impl))


struct lis_master_opt
{
	struct lis_option_descriptor parent;
	struct lis_master_item *item;

	intptr_t remote;

	void *value_msg;
};
#define LIS_MASTER_OPT_PRIVATE(opt) ((struct lis_master_opt *)(opt))


struct lis_master_scan_session
{
	struct lis_scan_session parent;
	struct lis_master_item *item;

	intptr_t remote;
};
#define LIS_MASTER_SCAN_SESSION_PRIVATE(session) ((struct lis_master_scan_session *)(session))


struct lis_master_item
{
	struct lis_item parent;
	struct lis_master_impl *impl;
	void *msg;
	intptr_t remote;
	bool root;

	struct {
		void *msg;
		struct lis_master_item *private;
		struct lis_item **ptrs;
	} children;

	struct {
		void *msg;
		struct lis_master_opt *private;
		struct lis_option_descriptor **ptrs;
	} opts;
};
#define LIS_MASTER_ITEM_PRIVATE(item) ((struct lis_master_item *)(item))


static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LIS_LOCK() do { \
		int __pthread_r; \
		__pthread_r = pthread_mutex_lock(&g_mutex); \
		assert(__pthread_r == 0); \
	} while(0)

#define LIS_UNLOCK() do { \
		int __pthread_r; \
		__pthread_r = pthread_mutex_unlock(&g_mutex); \
		assert(__pthread_r == 0); \
	} while(0)


static void master_cleanup(struct lis_api *impl);
static enum lis_error master_list_devices(
	struct lis_api *impl, enum lis_device_locations locs,
	struct lis_device_descriptor ***dev_infos);
static enum lis_error master_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item);


static struct lis_api g_master_impl_template = {
	.base_name = "dedicated_process",
	.cleanup = master_cleanup,
	.list_devices = master_list_devices,
	.get_device = master_get_device,
};


static enum lis_error master_item_get_children(struct lis_item *self, struct lis_item ***children);
static enum lis_error master_item_get_options(struct lis_item *self, struct lis_option_descriptor ***descs);
static enum lis_error master_item_scan_start(struct lis_item *self, struct lis_scan_session **session);
static void master_item_close(struct lis_item *self);


static struct lis_item g_master_item_template = {
	.get_children = master_item_get_children,
	.get_options = master_item_get_options,
	.scan_start = master_item_scan_start,
	.close = master_item_close,
};



static enum lis_error master_opt_get_value(
	struct lis_option_descriptor *self, union lis_value *value
);
static enum lis_error master_opt_set_value(
	struct lis_option_descriptor *self, union lis_value value, int *set_flags
);


static struct lis_option_descriptor g_master_opt_template = {
	.fn = {
		.get_value = master_opt_get_value,
		.set_value = master_opt_set_value,
	},
};



static enum lis_error master_session_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *parameters
);
static int master_session_end_of_feed(struct lis_scan_session *session);
static int master_session_end_of_page(struct lis_scan_session *session);
static enum lis_error master_session_scan_read(
	struct lis_scan_session *session, void *out_buffer,
	size_t *buffer_size
);
static void master_session_cancel(struct lis_scan_session *session);


static struct lis_scan_session g_master_session_template = {
	.get_scan_parameters = master_session_get_scan_parameters,
	.end_of_feed = master_session_end_of_feed,
	.end_of_page = master_session_end_of_page,
	.scan_read = master_session_scan_read,
	.cancel = master_session_cancel,
};


static void *log_thread(void *_pipes)
{
	struct lis_pipes *pipes = _pipes;
	enum lis_error err;
	enum lis_log_level lvl;
	const char *msg;

	lis_log_debug(
		"Logs pipe: Read: %d - Write: %d",
		pipes->sorted.logs[0], pipes->sorted.logs[1]
	);
	lis_log_debug(
		"Stderr pipe: Read: %d - Write: %d",
		pipes->sorted.stderr[0], pipes->sorted.stderr[1]
	);

	pipes->stderr = fdopen(pipes->sorted.stderr[0], "r");
	assert(pipes->stderr != NULL);

	lis_log_info("Log thread started");
	do {
		err = lis_protocol_log_read(pipes, &lvl, &msg);
		if (LIS_IS_OK(err)) {
			lis_log_raw(lvl, msg);
		}
	} while(LIS_IS_OK(err));

	lis_log_info(
		"Stopping log thread because: 0x%X, %s", err, lis_strerror(err)
	);
	fclose(pipes->stderr);

	return NULL;
}


static void configure_pipe(int pipe[2])
{
	unsigned int i;

	for (i = 0 ; i < 2 ; i++) {
		if (fcntl(pipe[i], F_SETFD, FD_CLOEXEC) < 0) {
			lis_log_warning(
				"fcntl(%d, F_SETFD, FD_CLOEXEC) failed: %d, %s",
				pipe[i], errno, strerror(errno)
			);
		}
	}
}


static void master_cleanup(struct lis_api *impl)
{
	struct lis_master_impl *private = LIS_MASTER_IMPL_PRIVATE(impl);
	int wstatus;
	enum lis_error err;
	int r;
	struct lis_msg msg = {
		.header = {
			.msg_type = LIS_MSG_API_CLEANUP,
			.err = LIS_OK,
		},
		.raw = { 0 },
	};

	LIS_LOCK();

	if (kill(private->worker, 0) >= 0) { // if worker is still alive
		lis_log_info("Requesting worker process to stop ...");
		err = lis_protocol_msg_write(
			private->pipes.sorted.msgs_m2w[1],
			&msg
		);
		if (LIS_IS_ERROR(err)) {
			lis_log_warning("Failed to send cleanup command");
		} else {
			lis_log_debug("Waiting for worker reply");
			err = lis_protocol_msg_read(
				private->pipes.sorted.msgs_w2m[0],
				&msg
			);
			if (LIS_IS_ERROR(err)) {
				lis_log_warning("Failed to receive cleanup reply");
			} else {
				lis_log_debug("Worker is going to stop");
				assert(msg.header.msg_type == LIS_MSG_API_CLEANUP);
				lis_protocol_msg_free(&msg);
			}
		}
	}

	lis_protocol_close(&private->pipes);

	if (waitpid(private->worker, &wstatus, 0) < 0) {
		lis_log_warning(
			"waitpid() failed: %d, %s",
			errno, strerror(errno)
		);
	} else if (WIFEXITED(wstatus)) {
		lis_log_info(
			"Worker process has ended with return code %d",
			WEXITSTATUS(wstatus)
		);
	} else {
		lis_log_warning(
			"Worker process has ended with status 0x%X", wstatus
		);
	}

	lis_log_info("Waiting for log thread to end ...");
	r = pthread_join(private->log_thread, NULL);
	if (r != 0) {
		lis_log_warning("pthread_join() failed: %d, %s", r, strerror(r));
	}

	FREE(private->data.list_devs.msg);
	FREE(private->data.list_devs.dev_ptrs);
	FREE(private->data.list_devs.devs);

	private->wrapped->cleanup(private->wrapped);

	FREE(private);

	LIS_UNLOCK();
}


static enum lis_error remote_call(
		struct lis_master_impl *private,
		const char *call_name,
		const struct lis_msg *msg_in,
		struct lis_msg *msg_out
	)
{
	enum lis_error err;

	err = lis_protocol_msg_write(private->pipes.sorted.msgs_m2w[1], msg_in);
	if (LIS_IS_ERROR(err)) {
		lis_log_error(
			"%s() failed: 0x%X, %s",
			call_name, err, lis_strerror(err)
		);
		return err;
	}

	memset(msg_out, 0, sizeof(*msg_out));

	err = lis_protocol_msg_read(private->pipes.sorted.msgs_w2m[0], msg_out);
	if (LIS_IS_ERROR(err)) {
		lis_log_error("%s() failed: 0x%X, %s", call_name, err, lis_strerror(err));
		return err;
	}

	return msg_out->header.err;
}


static enum lis_error master_list_devices(
	struct lis_api *impl, enum lis_device_locations locs,
	struct lis_device_descriptor ***devs)
{
	struct lis_master_impl *private = LIS_MASTER_IMPL_PRIVATE(impl);
	enum lis_error err = LIS_OK;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_API_LIST_DEVICES,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &locs,
			.iov_len = sizeof(locs),
		},
	};
	struct lis_msg msg_out;
	const void *ptr;
	int nb_devs = 0;
	int i;

	LIS_LOCK();

	*devs = NULL;

	FREE(private->data.list_devs.msg);
	FREE(private->data.list_devs.dev_ptrs);
	FREE(private->data.list_devs.devs);

	err = remote_call(private, "list_devices", &msg_in, &msg_out);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}
	private->data.list_devs.msg = msg_out.raw.iov_base;

	ptr = msg_out.raw.iov_base;
	lis_unpack(&ptr, "i", &nb_devs);

	private->data.list_devs.dev_ptrs = calloc(nb_devs + 1, sizeof(struct lis_device_descriptor *));
	private->data.list_devs.devs = calloc(nb_devs, sizeof(struct lis_device_descriptor));
	if (private->data.list_devs.dev_ptrs == NULL || private->data.list_devs.devs == NULL) {
		lis_log_error(
			"Out of memory (%d devs --> %p %p)",
			nb_devs, private->data.list_devs.dev_ptrs, private->data.list_devs.devs
		);
		err = LIS_ERR_NO_MEM;
		goto error;
	}

	for (i = 0 ; i < nb_devs ; i++) {
		private->data.list_devs.dev_ptrs[i] = &private->data.list_devs.devs[i];
		lis_unpack(
			&ptr, "ssss",
			&private->data.list_devs.devs[i].dev_id,
			&private->data.list_devs.devs[i].vendor,
			&private->data.list_devs.devs[i].model,
			&private->data.list_devs.devs[i].type
		);
	}

	*devs = private->data.list_devs.dev_ptrs;
	LIS_UNLOCK();
	return msg_out.header.err;

error:
	lis_protocol_msg_free(&msg_out);
	FREE(private->data.list_devs.dev_ptrs);
	FREE(private->data.list_devs.devs);
	LIS_UNLOCK();
	return err;
}


static enum lis_error master_get_device(
	struct lis_api *impl, const char *dev_id, struct lis_item **item)
{
	struct lis_master_impl *private = LIS_MASTER_IMPL_PRIVATE(impl);
	struct lis_master_item *out;
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_API_GET_DEVICE,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = (void*)dev_id,
			.iov_len = strlen(dev_id) + 1
		},
	};
	struct lis_msg msg_out;
	const void *ptr;

	LIS_LOCK();

	*item = NULL;

	err = remote_call(
		private, "get_device",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	out = calloc(1, sizeof(struct lis_master_item));
	if (out == NULL) {
		lis_log_error("Out of memory");
		goto error;
	}
	memcpy(&out->parent, &g_master_item_template, sizeof(out->parent));
	out->impl = private;
	out->msg = msg_out.raw.iov_base;
	out->root = true;

	ptr = msg_out.raw.iov_base;
	lis_unpack(
		&ptr, "sdp",
		&out->parent.name, &out->parent.type, &out->remote
	);

	*item = &out->parent;
	LIS_UNLOCK();
	return msg_out.header.err;

error:
	FREE(out);
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return err;
}


static void free_opts(struct lis_master_item *private)
{
	int i;

	if (private->opts.ptrs != NULL) {
		for (i = 0 ; private->opts.ptrs[i] != NULL ; i++) {
			FREE(private->opts.private[i].value_msg);
			if (private->opts.private[i].parent.constraint.type == LIS_CONSTRAINT_LIST) {
				FREE(private->opts.private[i].parent.constraint.possible.list.values);
			}
		}
		FREE(private->opts.msg);
		FREE(private->opts.private);
		FREE(private->opts.ptrs);
	}
}


static void free_children(struct lis_master_item *private)
{
	int i;

	if (private->children.private != NULL) {
		for (i = 0 ; private->children.ptrs[i] != NULL ; i++) {
			free_opts(&private->children.private[i]);
			free_children(&private->children.private[i]);
		}
	}
	FREE(private->children.msg);
	FREE(private->children.private);
	FREE(private->children.ptrs);
}


static enum lis_error master_item_get_children(struct lis_item *self, struct lis_item ***out_children)
{
	struct lis_master_item *private = LIS_MASTER_ITEM_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_ITEM_GET_CHILDREN,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr;
	int nb_children, i;

	LIS_LOCK();

	*out_children = NULL;

	err = remote_call(
		private->impl, "get_children",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	free_children(private);

	private->children.msg = msg_out.raw.iov_base;
	ptr = msg_out.raw.iov_base;

	lis_unpack(&ptr, "d", &nb_children);

	private->children.ptrs = calloc(nb_children + 1, sizeof(struct lis_item *));
	private->children.private = calloc(nb_children, sizeof(struct lis_master_item));
	if (private->children.ptrs == NULL || private->children.private == NULL) {
		lis_log_error("Out of memory");
		err = LIS_ERR_NO_MEM;
		goto error;
	}

	for (i = 0 ; i < nb_children ; i++) {
		private->children.ptrs[i] = &private->children.private[i].parent;
		memcpy(
			&private->children.private[i].parent,
			&g_master_item_template,
			sizeof(private->children.private[i].parent)
		);
		private->children.private[i].impl = private->impl;
		private->children.private[i].root = false;

		lis_unpack(
			&ptr, "sdp",
			&private->children.private[i].parent.name,
			&private->children.private[i].parent.type,
			&private->children.private[i].remote
		);
	}

	*out_children = private->children.ptrs;
	LIS_UNLOCK();
	return msg_out.header.err;

error:
	FREE(private->children.msg);
	FREE(private->children.private);
	FREE(private->children.ptrs);
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return err;
}


static void deserialize_range(
	const void **serialized,
	enum lis_value_type vtype, struct lis_value_range *range)
{
	lis_unpack(
		serialized, "vvv",
		vtype, &range->min,
		vtype, &range->max,
		vtype, &range->interval
	);
}


static enum lis_error deserialize_list(
	const void **serialized,
	enum lis_value_type vtype, struct lis_value_list *list)
{
	int i;

	lis_unpack(serialized, "d", &list->nb_values);

	list->values = calloc(list->nb_values, sizeof(union lis_value));
	if (list->values == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	for (i = 0 ; i < list->nb_values ; i++) {
		lis_unpack(serialized, "v", vtype, &list->values[i]);
	}

	return LIS_OK;
}


static enum lis_error deserialize_option(
	const void **serialized, struct lis_master_opt *opt,
	struct lis_master_item *item)
{
	enum lis_error err;

	memcpy(&opt->parent, &g_master_opt_template, sizeof(opt->parent));
	opt->item = item;

	lis_unpack(
		serialized,
		"psssdddd",
		&opt->remote,
		&opt->parent.name,
		&opt->parent.title,
		&opt->parent.desc,
		&opt->parent.capabilities,
		&opt->parent.value.type,
		&opt->parent.value.unit,
		&opt->parent.constraint.type
	);

	switch(opt->parent.constraint.type) {
		case LIS_CONSTRAINT_NONE:
			break;
		case LIS_CONSTRAINT_RANGE:
			deserialize_range(
				serialized,
				opt->parent.value.type,
				&opt->parent.constraint.possible.range
			);
			break;
		case LIS_CONSTRAINT_LIST:
			err = deserialize_list(
				serialized,
				opt->parent.value.type,
				&opt->parent.constraint.possible.list
			);
			if (LIS_IS_ERROR(err)) {
				return err;
			}
			break;
	}

	return LIS_OK;
}


static enum lis_error master_item_get_options(struct lis_item *self, struct lis_option_descriptor ***descs)
{
	struct lis_master_item *private = LIS_MASTER_ITEM_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_ITEM_GET_OPTIONS,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr;
	int nb_opts;
	int i;

	LIS_LOCK();

	*descs = NULL;

	err = remote_call(
		private->impl, "get_options",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	free_opts(private);

	ptr = msg_out.raw.iov_base;
	private->opts.msg = msg_out.raw.iov_base;

	lis_unpack(&ptr, "d", &nb_opts);

	private->opts.ptrs = calloc(nb_opts + 1, sizeof(struct lis_option_descriptor *));
	private->opts.private = calloc(nb_opts, sizeof(struct lis_master_opt));
	if (private->opts.ptrs == NULL || private->opts.private == NULL) {
		lis_log_error("Out of memory");
		err = LIS_ERR_NO_MEM;
		goto error;
	}

	for (i = 0 ; i < nb_opts ; i++) {
		err = deserialize_option(&ptr, &private->opts.private[i], private);
		if (LIS_IS_ERROR(err)) {
			goto error;
		}
		private->opts.ptrs[i] = &private->opts.private[i].parent;
	}

	*descs = private->opts.ptrs;
	LIS_UNLOCK();
	return msg_out.header.err;

error:
	FREE(private->opts.ptrs);
	FREE(private->opts.private);
	LIS_UNLOCK();
	return err;
}


static enum lis_error master_opt_get_value(
	struct lis_option_descriptor *self, union lis_value *value)
{
	struct lis_master_opt *private = LIS_MASTER_OPT_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_OPT_GET,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr_out;

	LIS_LOCK();

	err = remote_call(
		private->item->impl, "opt_get_value",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	FREE(private->value_msg);

	ptr_out = msg_out.raw.iov_base;
	private->value_msg = msg_out.raw.iov_base;

	lis_unpack(&ptr_out, "v", self->value.type, value);
	LIS_UNLOCK();
	return msg_out.header.err;
}


static enum lis_error master_opt_set_value(
	struct lis_option_descriptor *self, union lis_value value, int *set_flags)
{
	struct lis_master_opt *private = LIS_MASTER_OPT_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_OPT_SET,
			.err = LIS_OK,
		},
		.raw = { 0 },
	};
	struct lis_msg msg_out;
	void *ptr_in;
	const void *ptr_out;

	LIS_LOCK();

	msg_in.raw.iov_len = lis_compute_packed_size(
		"pv", private->remote, self->value.type, value
	);
	msg_in.raw.iov_base = malloc(msg_in.raw.iov_len);
	if (msg_in.raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		LIS_UNLOCK();
		return LIS_ERR_NO_MEM;
	}
	ptr_in = msg_in.raw.iov_base;
	lis_pack(&ptr_in, "pv", private->remote, self->value.type, value);

	err = remote_call(
		private->item->impl, "opt_set_value",
		&msg_in, &msg_out
	);
	lis_protocol_msg_free(&msg_in);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	ptr_out = msg_out.raw.iov_base;
	lis_unpack(&ptr_out, "d", set_flags);
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return msg_out.header.err;
}


static enum lis_error master_item_scan_start(struct lis_item *self, struct lis_scan_session **out_session)
{
	struct lis_master_item *private = LIS_MASTER_ITEM_PRIVATE(self);
	struct lis_master_scan_session *session;
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_ITEM_SCAN_START,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr_out;

	LIS_LOCK();

	err = remote_call(private->impl, "item_scan_start", &msg_in, &msg_out);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	session = calloc(1, sizeof(struct lis_master_scan_session));
	if (session == NULL) {
		lis_log_error("Out of memory");
		// TODO: Closing session
		return LIS_ERR_NO_MEM;
	}
	memcpy(&session->parent, &g_master_session_template, sizeof(session->parent));
	session->item = private;

	ptr_out = msg_out.raw.iov_base;
	lis_unpack(&ptr_out, "p", &session->remote);

	*out_session = &session->parent;
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return msg_out.header.err;
}


static enum lis_error master_session_get_scan_parameters(
	struct lis_scan_session *self,
	struct lis_scan_parameters *parameters)
{
	struct lis_master_scan_session *private = LIS_MASTER_SCAN_SESSION_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_SESSION_GET_SCAN_PARAMETERS,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;

	LIS_LOCK();

	err = remote_call(
		private->item->impl, "session_get_scan_parameters",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	assert(msg_out.raw.iov_len == sizeof(struct lis_scan_parameters));
	memcpy(parameters, msg_out.raw.iov_base, sizeof(*parameters));
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return msg_out.header.err;
}


static int master_session_end_of_feed(struct lis_scan_session *self)
{
	struct lis_master_scan_session *private = LIS_MASTER_SCAN_SESSION_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_SESSION_END_OF_FEED,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr_out;
	int r;

	LIS_LOCK();

	err = remote_call(
		private->item->impl, "session_end_of_feed",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return 1;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return 1;
	}

	ptr_out = msg_out.raw.iov_base;
	lis_unpack(&ptr_out, "i", &r);
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return r;
}


static int master_session_end_of_page(struct lis_scan_session *self)
{
	struct lis_master_scan_session *private = LIS_MASTER_SCAN_SESSION_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_SESSION_END_OF_PAGE,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;
	const void *ptr_out;
	int r;

	LIS_LOCK();

	err = remote_call(
		private->item->impl, "session_end_of_page",
		&msg_in, &msg_out
	);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return 1;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return 1;
	}

	ptr_out = msg_out.raw.iov_base;
	lis_unpack(&ptr_out, "i", &r);
	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return r;
}


static enum lis_error master_session_scan_read(
	struct lis_scan_session *self, void *out_buffer,
	size_t *buffer_size)
{
	struct lis_master_scan_session *private = LIS_MASTER_SCAN_SESSION_PRIVATE(self);
	enum lis_error err;
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_SESSION_SCAN_READ,
			.err = LIS_OK,
		},
		.raw = { 0 },
	};
	struct lis_msg msg_out;
	void *ptr_in;

	LIS_LOCK();

	msg_in.raw.iov_len = lis_compute_packed_size(
		"pd", private->remote, (int)(*buffer_size)
	);
	msg_in.raw.iov_base = malloc(msg_in.raw.iov_len);
	if (msg_in.raw.iov_base == NULL) {
		lis_log_error("Out of memory");
		LIS_UNLOCK();
		return LIS_ERR_NO_MEM;
	}
	ptr_in = msg_in.raw.iov_base;
	lis_pack(&ptr_in, "pd", private->remote, (int)(*buffer_size));

	err = remote_call(
		private->item->impl, "session_scan_read",
		&msg_in, &msg_out
	);
	lis_protocol_msg_free(&msg_in);
	if (LIS_IS_ERROR(err)) {
		LIS_UNLOCK();
		return err;
	}
	if (LIS_IS_ERROR(msg_out.header.err)) {
		LIS_UNLOCK();
		return msg_out.header.err;
	}

	memcpy(
		out_buffer, msg_out.raw.iov_base,
		MIN((*buffer_size), msg_out.raw.iov_len)
	);
	*buffer_size = msg_out.raw.iov_len;

	lis_protocol_msg_free(&msg_out);
	LIS_UNLOCK();
	return msg_out.header.err;
}


static void master_session_cancel(struct lis_scan_session *self)
{
	struct lis_master_scan_session *private = LIS_MASTER_SCAN_SESSION_PRIVATE(self);
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_SESSION_CANCEL,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;

	LIS_LOCK();

	remote_call(private->item->impl, "scan_session_cancel", &msg_in, &msg_out);
	lis_protocol_msg_free(&msg_out);
	FREE(private);

	LIS_UNLOCK();
}


static void master_item_close(struct lis_item *self)
{
	struct lis_master_item *private = LIS_MASTER_ITEM_PRIVATE(self);
	struct lis_msg msg_in = {
		.header = {
			.msg_type = LIS_MSG_ITEM_CLOSE,
			.err = LIS_OK,
		},
		.raw = {
			.iov_base = &private->remote,
			.iov_len = sizeof(private->remote),
		},
	};
	struct lis_msg msg_out;

	LIS_LOCK();

	remote_call(private->impl, "item_close", &msg_in, &msg_out);
	lis_protocol_msg_free(&msg_out);

	free_opts(private);
	free_children(private);
	FREE(private->msg);
	if (private->root) {
		FREE(private);
	}

	LIS_UNLOCK();
}


enum lis_error lis_api_workaround_dedicated_process(
		struct lis_api *to_wrap, struct lis_api **out_impl
	)
{
	struct lis_master_impl *private;
	unsigned int i;
	int r;

	private = calloc(1, sizeof(struct lis_master_impl));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	private->wrapped = to_wrap;

	lis_log_info("Creating pipes ...");
	for (i = 0 ; i < LIS_COUNT_OF(private->pipes.all); i++) {
		if (pipe(private->pipes.all[i]) < 0) {
			lis_log_error("pipe() failed: %d, %s", errno, strerror(errno));
			goto err;
		}
		lis_log_debug(
			"Pipe: Read: %d - Write: %d",
			private->pipes.all[i][0], private->pipes.all[i][1]
		);
		configure_pipe(private->pipes.all[i]);
	}
	lis_log_info("Forking ...");
	private->worker = fork();
	if (private->worker < 0) {
		lis_log_error("fork() failed: %d, %s", errno, strerror(errno));
		goto err;
	}

	if (private->worker == 0) {
		// we are the worker processus
		close(private->pipes.sorted.msgs_m2w[1]);
		private->pipes.sorted.msgs_m2w[1] = -1;
		close(private->pipes.sorted.msgs_w2m[0]);
		private->pipes.sorted.msgs_w2m[0] = -1;
		close(private->pipes.sorted.logs[0]);
		private->pipes.sorted.logs[0] = -1;
		close(private->pipes.sorted.stderr[0]);
		private->pipes.sorted.stderr[0] = -1;

		lis_worker_main(to_wrap, &private->pipes);
		abort(); // lis_worker_main() must never return
	}

	// we are the master processus
	close(private->pipes.sorted.msgs_m2w[0]);
	private->pipes.sorted.msgs_m2w[0] = -1;
	close(private->pipes.sorted.msgs_w2m[1]);
	private->pipes.sorted.msgs_w2m[1] = -1;
	close(private->pipes.sorted.logs[1]);
	private->pipes.sorted.logs[1] = -1;
	close(private->pipes.sorted.stderr[1]);
	private->pipes.sorted.stderr[1] = -1;

	lis_log_info("Child process PID: %u", (int)private->worker);

	lis_log_info("Starting log thread ...");
	r = pthread_create(&private->log_thread, NULL, log_thread, &private->pipes);
	if (r != 0) {
		lis_log_warning(
			"Failed to create log thread: %d, %s",
			r, strerror(r)
		);
	}

	memcpy(&private->parent, &g_master_impl_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;

	*out_impl = &private->parent;
	return LIS_OK;

err:
	for (i = 0 ; i < LIS_COUNT_OF(private->pipes.all); i++) {
		if (private->pipes.all[i][0] > 0) {
			close(private->pipes.all[i][0]);
		}
		if (private->pipes.all[i][1] > 0) {
			close(private->pipes.all[i][1]);
		}
	}
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}
