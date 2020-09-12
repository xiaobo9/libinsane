#ifndef __LIBINSANE_WORKAROUND_DEDICATED_PROCESS_PROTOCOL_H
#define __LIBINSANE_WORKAROUND_DEDICATED_PROCESS_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/uio.h>
#include <sys/types.h>

#include <libinsane/error.h>
#include <libinsane/log.h>


/* Both processes communicate through a pipe. Since both processes run
 * on the same host, we don't have to worry about annoying details things like
 * structure padding.
 *
 * All messages starts with:
 * - enum lis_msg_type : message_type
 * - enum lis_error : LIS_OK unless an error occured
 * - size_t : message_size if lis_error == LIS_OK
 */

enum lis_msg_type
{
	LIS_MSG_API_CLEANUP = 0,
	LIS_MSG_API_LIST_DEVICES,
	LIS_MSG_API_GET_DEVICE,

	LIS_MSG_ITEM_GET_CHILDREN,
	LIS_MSG_ITEM_GET_OPTIONS,
	LIS_MSG_ITEM_SCAN_START,
	LIS_MSG_ITEM_CLOSE,

	LIS_MSG_OPT_GET,
	LIS_MSG_OPT_SET,

	LIS_MSG_SESSION_GET_SCAN_PARAMETERS,
	LIS_MSG_SESSION_END_OF_FEED,
	LIS_MSG_SESSION_END_OF_PAGE,
	LIS_MSG_SESSION_SCAN_READ,
	LIS_MSG_SESSION_CANCEL,
};


struct lis_msg
{
	struct {
		enum lis_msg_type msg_type;
		enum lis_error err;
	} header;

	struct iovec raw;
};


struct lis_pipes
{
	union {
		struct {
			int msgs_m2w[2]; /* messages ; query (master -> worker) */
			int msgs_w2m[2]; /* messages ; reply (worker -> master) */
			int logs[2]; /* worker to master only ; prefixed by log level + msg len */
			int stderr[2]; /* worker to master only */
		} sorted;
		int all[4][2];
	};

	char log_buf[1024]; // to avoid a malloc on each log line

	struct {
		char buf[1024]; // to avoid a malloc() on each stderr line
		ssize_t current;
		ssize_t total;
	} stderr;
};


/*!
 * Reads a struct lis_msg from the specified pipe file descriptor.
 * Takes care of deserializing the content.
 *
 * \param[in] fd file descriptor from which to read
 * \param[out] out_msg raw message
 *
 * Read message must be freed with lis_msg_free(msg);
 */
enum lis_error lis_protocol_msg_read(int fd, struct lis_msg *out_msg);


/*!
 * Writes a message to the specified pipe file descriptor.
 * Takes care of serializing the content.
 *
 * \param[in] fd file descriptor on which the message must be written
 * \param[in] in_msg raw message
 */
enum lis_error lis_protocol_msg_write(int fd, const struct lis_msg *in_msg);


/*!
 * Transmit a log message.
 * Always from worker to master.
 */
enum lis_error lis_protocol_log_write(struct lis_pipes *pipes, enum lis_log_level lvl, const char *msg);

/*!
 * Read a log message.
 *
 * \param[out] msg message read. valid until the next call to lis_protocol_log_read()
 *
 * Always from worker to master.
 * May come from the stderr output of the worker process.
 */
enum lis_error lis_protocol_log_read(struct lis_pipes *pipes, enum lis_log_level *lvl, const char **msg);

void lis_protocol_msg_free(struct lis_msg *msg);

/*!
 * Closes all the pipes
 */
void lis_protocol_close(struct lis_pipes *pipes);

#endif
