#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <libinsane/log.h>
#include <libinsane/workarounds.h>
#include <libinsane/util.h>


typedef void (*cb_t)(void *data);

struct op {
	cb_t cb;
	void *data;

	pthread_cond_t cond;

	struct op *next;
};


struct dt_impl_private {
	struct lis_api parent;
	struct lis_api *wrapped;

	pthread_t mainloop;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct op *first_op;
	struct op *last_op;
};
#define DT_IMPL_PRIVATE(impl) ((struct dt_impl_private *)(impl))


struct dt_opt_private {
	struct lis_option_descriptor parent;
	struct lis_option_descriptor *wrapped;
	struct dt_impl_private *impl;
};
#define DT_OPT_PRIVATE(opt) ((struct dt_opt_private *)(opt))


struct dt_item_private {
	struct lis_item parent;
	struct lis_item *wrapped;
	struct dt_impl_private *impl;

	struct dt_item_private *children;
	struct lis_item **children_ptrs;

	struct dt_opt_private *opts;
	struct lis_option_descriptor **opts_ptrs;

	struct dt_scan_session_private *session;
};
#define DT_ITEM_PRIVATE(impl) ((struct dt_item_private *)(impl))


struct dt_scan_session_private {
	struct lis_scan_session parent;
	struct lis_scan_session *wrapped;
	struct dt_item_private *item;
	struct dt_impl_private *impl;
};
#define DT_SCAN_SESSION_PRIVATE(session) ((struct dt_scan_session_private *)(session))


static void dt_impl_cleanup(struct lis_api *impl);
static enum lis_error dt_impl_list_devices(
		struct lis_api *impl, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	);
static enum lis_error dt_impl_get_device(
		struct lis_api *impl, const char *dev_id, struct lis_item **item
	);

static struct lis_api g_impl_template = {
	.cleanup = dt_impl_cleanup,
	.list_devices = dt_impl_list_devices,
	.get_device = dt_impl_get_device,
};


static enum lis_error dt_item_get_children(
		struct lis_item *self, struct lis_item ***children
	);
static enum lis_error dt_item_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	);
static enum lis_error dt_item_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	);
static void dt_item_root_close(struct lis_item *self);
static void dt_item_child_close(struct lis_item *self);


static struct lis_item g_item_root_template = {
	.get_children = dt_item_get_children,
	.get_options = dt_item_get_options,
	.scan_start = dt_item_scan_start,
	.close = dt_item_root_close,
};


static struct lis_item g_item_child_template = {
	.get_children = dt_item_get_children,
	.get_options = dt_item_get_options,
	.scan_start = dt_item_scan_start,
	.close = dt_item_child_close,
};


static enum lis_error dt_opt_get_value(
		struct lis_option_descriptor *self, union lis_value *value
	);
static enum lis_error dt_opt_set_value(
		struct lis_option_descriptor *self, union lis_value value,
		int *set_flags
	);


static enum lis_error dt_scan_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *parameters
	);
static int dt_scan_end_of_feed(struct lis_scan_session *self);
static int dt_scan_end_of_page(struct lis_scan_session *self);
static enum lis_error dt_scan_read(
	struct lis_scan_session *self, void *out_buffer, size_t *buffer_size
);
static void dt_scan_cancel(struct lis_scan_session *self);


static struct lis_scan_session g_scan_session_template = {
	.get_scan_parameters = dt_scan_get_scan_parameters,
	.end_of_feed = dt_scan_end_of_feed,
	.end_of_page = dt_scan_end_of_page,
	.scan_read = dt_scan_read,
	.cancel = dt_scan_cancel,
};


static void *main_loop(void *arg)
{
	struct dt_impl_private *private = arg;
	int ret;
	struct op *op;

	ret = pthread_mutex_lock(&private->mutex);
	assert(ret == 0);

	lis_log_info("Dedicated thread started");

	while(1) {
		while (private->first_op != NULL) {
			op = private->first_op;
			private->first_op = private->first_op->next;
			if (private->last_op == op) {
				private->last_op = NULL;
			}

			ret = pthread_mutex_unlock(&private->mutex);
			assert(ret == 0);

			op->cb(op->data);

			ret = pthread_mutex_lock(&private->mutex);
			assert(ret == 0);

			ret = pthread_cond_broadcast(&op->cond);
			assert(ret == 0);
		}

		ret = pthread_cond_wait(&private->cond, &private->mutex);
		assert(ret == 0);
	}

	/* never reached */
	assert(0);
	return NULL;
}


static void run(struct dt_impl_private *private, cb_t cb, void *data)
{
	struct op op = {
		.cb = cb,
		.data = data,
		.cond = PTHREAD_COND_INITIALIZER,
		.next = NULL,
	};
	int ret;

	ret = pthread_mutex_lock(&private->mutex);
	assert(ret == 0);

	if (private->last_op == NULL) {
		private->first_op = &op;
		private->last_op = &op;
	} else {
		private->last_op->next = &op;
	}

	ret = pthread_cond_broadcast(&private->cond);
	assert(ret == 0);

	ret = pthread_cond_wait(&op.cond, &private->mutex);
	assert(ret == 0);

	ret = pthread_mutex_unlock(&private->mutex);
	assert(ret == 0);

	ret = pthread_cond_destroy(&op.cond);
	assert(ret == 0);
}


static void real_impl_cleanup(void *_data)
{
	struct dt_impl_private *private = _data;
	private->wrapped->cleanup(private->wrapped);
	pthread_exit(NULL);
}


static void dt_impl_cleanup(struct lis_api *self)
{
	struct dt_impl_private *private = DT_IMPL_PRIVATE(self);
	struct op op = {
		.cb = real_impl_cleanup,
		.data = private,
		.next = NULL,
	};
	int ret;

	lis_log_info("Stopping dedicated thread");

	ret = pthread_mutex_lock(&private->mutex);
	assert(ret == 0);

	if (private->last_op == NULL) {
		private->first_op = &op;
		private->last_op = &op;
	} else {
		private->last_op->next = &op;
	}

	ret = pthread_cond_broadcast(&private->cond);
	assert(ret == 0);

	ret = pthread_mutex_unlock(&private->mutex);
	assert(ret == 0);

	/* do not wait for the condition of 'op'. It will never
	 * be signaled.
	 * Instead just wait for the thread to end.
	 */

	ret = pthread_join(private->mainloop, NULL);
	assert(ret == 0);

	lis_log_info("Dedicated thread stopped");

	ret = pthread_cond_destroy(&private->cond);
	assert(ret == 0);

	ret = pthread_mutex_destroy(&private->mutex);
	assert(ret == 0);

	FREE(private);
}


struct impl_list_devices_data {
	struct dt_impl_private *private;
	enum lis_device_locations locs;
	struct lis_device_descriptor ***dev_infos;

	enum lis_error ret;
};


static void real_impl_list_devices(void *_data)
{
	struct impl_list_devices_data *data = _data;

	data->ret = data->private->wrapped->list_devices(
		data->private->wrapped,
		data->locs,
		data->dev_infos
	);
}


static enum lis_error dt_impl_list_devices(
		struct lis_api *self, enum lis_device_locations locs,
		struct lis_device_descriptor ***dev_infos
	)
{
	struct dt_impl_private *private = DT_IMPL_PRIVATE(self);
	struct impl_list_devices_data data = {
		.private = private,
		.locs = locs,
		.dev_infos = dev_infos,
	};

	run(private, real_impl_list_devices, &data);
	return data.ret;
}


struct impl_get_device_data {
	struct dt_impl_private *private;
	const char *dev_id;
	struct lis_item **item;

	enum lis_error ret;
};


static void real_impl_get_device(void *_data)
{
	struct impl_get_device_data *data = _data;
	struct dt_item_private *item;

	item = calloc(1, sizeof(struct dt_item_private));
	if (item == NULL) {
		lis_log_error("Out of memory");
		data->ret = LIS_ERR_NO_MEM;
		return;
	}

	data->ret = data->private->wrapped->get_device(
		data->private->wrapped,
		data->dev_id,
		&item->wrapped
	);
	if (LIS_IS_ERROR(data->ret)) {
		FREE(item);
		return;
	}

	memcpy(&item->parent, &g_item_root_template, sizeof(item->parent));
	item->impl = data->private;
	item->parent.name = item->wrapped->name;
	item->parent.type = item->wrapped->type;
	*(data->item) = &item->parent;
}


static enum lis_error dt_impl_get_device(
		struct lis_api *self, const char *dev_id, struct lis_item **item
	)
{
	struct dt_impl_private *private = DT_IMPL_PRIVATE(self);
	struct impl_get_device_data data = {
		.private = private,
		.dev_id = dev_id,
		.item = item,
	};

	run(private, real_impl_get_device, &data);
	return data.ret;
}


struct dt_item_get_children_data {
	struct dt_item_private *private;
	struct lis_item ***children;

	enum lis_error ret;
};


static void real_item_get_children(void *_data)
{
	struct dt_item_get_children_data *data = _data;
	struct lis_item **to_wrap;
	int nb_children, i;

	data->ret = data->private->wrapped->get_children(
		data->private->wrapped,
		&to_wrap
	);
	if (LIS_IS_ERROR(data->ret)) {
		return;
	}

	for(nb_children = 0 ; to_wrap[nb_children] != NULL ; nb_children++) { }

	if (nb_children == 0) {
		*(data->children) = to_wrap;
		return;
	}

	data->private->children = calloc(
		nb_children, sizeof(struct dt_item_private)
	);
	data->private->children_ptrs = calloc(
		nb_children + 1, sizeof(struct lis_item *)
	);

	if (data->private->children == NULL || data->private->children_ptrs == NULL) {
		FREE(data->private->children);
		FREE(data->private->children_ptrs);
		lis_log_error("Out of memory");
		data->ret = LIS_ERR_NO_MEM;
		return;
	}

	for (i = 0 ; to_wrap[i] != NULL ; i++) {
		data->private->children_ptrs[i] = &data->private->children[i].parent;
		data->private->children[i].impl = data->private->impl;
		data->private->children[i].wrapped = to_wrap[i];
		memcpy(
			&data->private->children[i].parent,
			&g_item_child_template,
			sizeof(data->private->children[i].parent)
		);
		data->private->children[i].parent.name = to_wrap[i]->name;
		data->private->children[i].parent.type = to_wrap[i]->type;
	}

	*(data->children) = data->private->children_ptrs;
}


static enum lis_error dt_item_get_children(
		struct lis_item *self, struct lis_item ***children
	)
{
	struct dt_item_private *private = DT_ITEM_PRIVATE(self);
	struct dt_item_get_children_data data = {
		.private = private,
		.children = children,
	};
	run(private->impl, real_item_get_children, &data);
	return data.ret;
}


struct dt_item_get_options_data {
	struct dt_item_private *private;
	struct lis_option_descriptor ***descs;

	enum lis_error ret;
};


static void real_item_get_options(void *_data)
{
	struct dt_item_get_options_data *data = _data;
	struct lis_option_descriptor **to_wrap;
	int nb_opts, i;

	data->ret = data->private->wrapped->get_options(
		data->private->wrapped, &to_wrap
	);
	if (LIS_IS_ERROR(data->ret)) {
		return;
	}

	for (nb_opts = 0 ; to_wrap[nb_opts] != NULL ; nb_opts++) { }

	if (nb_opts == 0) {
		*(data->descs) = to_wrap;
		return;
	}

	data->private->opts = calloc(nb_opts, sizeof(struct dt_opt_private));
	data->private->opts_ptrs = calloc(
		nb_opts + 1, sizeof(struct lis_option_descriptor *)
	);
	if (data->private->opts == NULL || data->private->opts_ptrs == NULL) {
		FREE(data->private->opts);
		FREE(data->private->opts_ptrs);
		lis_log_error("Out of memory");
		data->ret = LIS_ERR_NO_MEM;
		return;
	}

	for (i = 0 ; to_wrap[i] != NULL ; i++) {
		data->private->opts_ptrs[i] = &data->private->opts[i].parent;
		memcpy(
			&data->private->opts[i].parent,
			to_wrap[i],
			sizeof(data->private->opts[i].parent)
		);
		data->private->opts[i].parent.fn.get_value = dt_opt_get_value;
		data->private->opts[i].parent.fn.set_value = dt_opt_set_value;
		data->private->opts[i].wrapped = to_wrap[i];
		data->private->opts[i].impl = data->private->impl;
	}

	*(data->descs) = data->private->opts_ptrs;
}


static enum lis_error dt_item_get_options(
		struct lis_item *self, struct lis_option_descriptor ***descs
	)
{
	struct dt_item_private *private = DT_ITEM_PRIVATE(self);
	struct dt_item_get_options_data data = {
		.private = private,
		.descs = descs,
	};
	run(private->impl, real_item_get_options, &data);
	return data.ret;
}


struct dt_item_scan_start_data {
	struct dt_item_private *private;
	struct lis_scan_session **session;

	enum lis_error ret;
};


static void real_item_scan_start(void *_data)
{
	struct dt_item_scan_start_data *data = _data;
	struct dt_scan_session_private *session;

	if (data->private->session != NULL) {
		FREE(data->private->session);
	}

	session = calloc(1, sizeof(struct dt_scan_session_private));
	data->private->session = session;
	if (session == NULL) {
		lis_log_error("Out of memory");
		data->ret = LIS_ERR_NO_MEM;
		return;
	}
	session->item = data->private;

	data->ret = data->private->wrapped->scan_start(
		data->private->wrapped,
		&session->wrapped
	);
	if (LIS_IS_ERROR(data->ret)) {
		return;
	}
	memcpy(
		&session->parent, &g_scan_session_template,
		sizeof(session->parent)
	);
	session->impl = data->private->impl;

	*(data->session) = &session->parent;
}


static enum lis_error dt_item_scan_start(
		struct lis_item *self, struct lis_scan_session **session
	)
{
	struct dt_item_private *private = DT_ITEM_PRIVATE(self);
	struct dt_item_scan_start_data data = {
		.private = private,
		.session = session,
	};
	run(private->impl, real_item_scan_start, &data);
	return data.ret;
}


static void close_item(struct dt_item_private *item)
{
	int i;

	if (item->children_ptrs != NULL) {
		for (i = 0 ; item->children_ptrs[i] != NULL ; i++) {
			close_item(&item->children[i]);
		}
		FREE(item->children_ptrs);
		FREE(item->children);
	}
	FREE(item->session);
}


static void real_item_root_close(void *_data)
 {
	struct dt_item_private *private = _data;
	private->wrapped->close(private->wrapped);
	close_item(private);
	FREE(private);
}


static void dt_item_root_close(struct lis_item *self)
{
	struct dt_item_private *private = DT_ITEM_PRIVATE(self);
	run(private->impl, real_item_root_close, private);
}


static void dt_item_child_close(struct lis_item *self)
{
	LIS_UNUSED(self);
	// Nothing to do
}


struct dt_opt_get_value_data {
	struct dt_opt_private *private;
	union lis_value *value;

	enum lis_error ret;
};


static void real_opt_get_value(void *_data)
{
	struct dt_opt_get_value_data *data = _data;
	data->ret = data->private->wrapped->fn.get_value(
		data->private->wrapped,
		data->value
	);
}


static enum lis_error dt_opt_get_value(
		struct lis_option_descriptor *self, union lis_value *value
	)
{
	struct dt_opt_private *private = DT_OPT_PRIVATE(self);
	struct dt_opt_get_value_data data = {
		.private = private,
		.value = value,
	};
	run(private->impl, real_opt_get_value, &data);
	return data.ret;
}


struct dt_opt_set_value_data {
	struct dt_opt_private *private;
	union lis_value value;
	int *set_flags;

	enum lis_error ret;
};


static void real_opt_set_value(void *_data)
{
	struct dt_opt_set_value_data *data = _data;
	data->ret = data->private->wrapped->fn.set_value(
		data->private->wrapped,
		data->value,
		data->set_flags
	);
}


static enum lis_error dt_opt_set_value(
		struct lis_option_descriptor *self, union lis_value value,
		int *set_flags
	)
{
	struct dt_opt_private *private = DT_OPT_PRIVATE(self);
	struct dt_opt_set_value_data data = {
		.private = private,
		.value = value,
		.set_flags = set_flags,
	};
	run(private->impl, real_opt_set_value, &data);
	return data.ret;
}


struct dt_get_scan_parameters_data {
	struct dt_scan_session_private *private;
	struct lis_scan_parameters *parameters;

	enum lis_error ret;
};


static void real_scan_get_scan_parameters(void *_data)
{
	struct dt_get_scan_parameters_data *data = _data;

	data->ret = data->private->wrapped->get_scan_parameters(
		data->private->wrapped,
		data->parameters
	);
}


static enum lis_error dt_scan_get_scan_parameters(
		struct lis_scan_session *self,
		struct lis_scan_parameters *parameters
	)
{
	struct dt_scan_session_private *private = DT_SCAN_SESSION_PRIVATE(self);
	struct dt_get_scan_parameters_data data = {
		.private = private,
		.parameters = parameters,
	};
	run(private->impl, real_scan_get_scan_parameters, &data);
	return data.ret;
}


struct dt_scan_end_of_data {
	struct dt_scan_session_private *private;
	int ret;
};


static void real_scan_end_of_feed(void *_data)
{
	struct dt_scan_end_of_data *data = _data;
	data->ret = data->private->wrapped->end_of_feed(
		data->private->wrapped
	);
}


static int dt_scan_end_of_feed(struct lis_scan_session *self)
{
	struct dt_scan_session_private *private = DT_SCAN_SESSION_PRIVATE(self);
	struct dt_scan_end_of_data data = {
		.private = private,
	};
	run(private->impl, real_scan_end_of_feed, &data);
	return data.ret;
}


static void real_scan_end_of_page(void *_data)
{
	struct dt_scan_end_of_data *data = _data;
	data->ret = data->private->wrapped->end_of_page(
		data->private->wrapped
	);
}


static int dt_scan_end_of_page(struct lis_scan_session *self)
{
	struct dt_scan_session_private *private = DT_SCAN_SESSION_PRIVATE(self);
	struct dt_scan_end_of_data data = {
		.private = private,
	};
	run(private->impl, real_scan_end_of_page, &data);
	return data.ret;
}


struct dt_scan_read_data {
	struct dt_scan_session_private *private;
	void *out_buffer;
	size_t *buffer_size;

	enum lis_error ret;
};


static void real_scan_read(void *_data)
{
	struct dt_scan_read_data *data = _data;
	data->ret = data->private->wrapped->scan_read(
		data->private->wrapped,
		data->out_buffer,
		data->buffer_size
	);
}


static enum lis_error dt_scan_read(
		struct lis_scan_session *self,
		void *out_buffer, size_t *buffer_size
	)
{
	struct dt_scan_session_private *private = DT_SCAN_SESSION_PRIVATE(self);
	struct dt_scan_read_data data = {
		.private = private,
		.out_buffer = out_buffer,
		.buffer_size = buffer_size,
	};
	run(private->impl, real_scan_read, &data);
	return data.ret;
}


static void real_scan_cancel(void *_data)
{
	struct dt_scan_session_private *private = _data;
	private->wrapped->cancel(private->wrapped);
}



static void dt_scan_cancel(struct lis_scan_session *self)
{
	struct dt_scan_session_private *private = DT_SCAN_SESSION_PRIVATE(self);
	run(private->impl, real_scan_cancel, private);
	private->item->session = NULL;
	FREE(private);
}


enum lis_error lis_api_workaround_dedicated_thread(
		struct lis_api *to_wrap, struct lis_api **impl
	)
{
	struct dt_impl_private *private;
	int ret;

	private = calloc(1, sizeof(struct dt_impl_private));
	if (private == NULL) {
		lis_log_error("Out of memory");
		return LIS_ERR_NO_MEM;
	}

	private->wrapped = to_wrap;
	memcpy(&private->parent, &g_impl_template, sizeof(private->parent));
	private->parent.base_name = to_wrap->base_name;

	ret = pthread_mutex_init(&private->mutex, NULL);
	assert(ret == 0);
	ret = pthread_cond_init(&private->cond, NULL);
	assert(ret == 0);
	ret = pthread_create(&private->mainloop, NULL, main_loop, private);
	assert(ret == 0);

	*impl = &private->parent;
	return LIS_OK;
}
