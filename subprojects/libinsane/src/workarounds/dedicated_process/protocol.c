#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <libinsane/log.h>
#include <libinsane/util.h>

#include "protocol.h"


// #define PROTOCOL_DEBUG

#ifndef PROTOCOL_DEBUG

#define lis_read(a, b, c) read((a), (b), (c))
#define lis_write(a, b, c) write((a), (b), (c))

#else

static ssize_t lis_read(int fd, void *buf, size_t count)
{
	ssize_t r;

	r = read(fd, buf, count);
	lis_hexdump("read", buf, r);
	return r;
}


static ssize_t lis_write(int fd, const void *buf, size_t count)
{
	ssize_t w;

	w = write(fd, buf, count);
	lis_hexdump("write", buf, w);
	return w;
}

#endif


enum lis_error lis_protocol_msg_read(int fd, struct lis_msg *msg)
{
	ssize_t r;

	memset(msg, 0, sizeof(*msg));

	r = lis_read(fd, &msg->header, sizeof(msg->header));
	if (r != sizeof(msg->header)) {
		lis_log_error(
			"read() failed: fd=%d, r=%zd ; %d, %s",
			fd, r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	if (LIS_IS_ERROR(msg->header.err)) {
		return msg->header.err;
	}

	r = lis_read(fd, &msg->raw.iov_len, sizeof(msg->raw.iov_len));
	if (r != sizeof(msg->raw.iov_len)) {
		lis_log_error(
			"read() failed (1): fd=%d, r=%zd ; %d, %s",
			fd, r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	if (msg->raw.iov_len > 0) {
		msg->raw.iov_base = malloc(msg->raw.iov_len);
		if (msg->raw.iov_base == NULL) {
			lis_log_error(
				"Out of memory (requested: %zu)\n", msg->raw.iov_len
			);
			return LIS_ERR_NO_MEM;
		}

		r = lis_read(fd, msg->raw.iov_base, msg->raw.iov_len);
		if (r != (ssize_t)msg->raw.iov_len) {
			lis_log_error("read() failed (2): r=%zd ; %d, %s", r, errno, strerror(errno));
			return LIS_ERR_IO_ERROR;
		}
	}

	return LIS_OK;
}

enum lis_error lis_protocol_msg_write(int fd, const struct lis_msg *msg)
{
	ssize_t r;

	r = lis_write(fd, &msg->header, sizeof(msg->header));
	if (r != sizeof(msg->header)) {
		lis_log_error(
			"write() failed: fd=%d, r=%zd ; %d, %s",
			fd, r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	if (LIS_IS_ERROR(msg->header.err)) {
		return LIS_OK;
	}

	r = lis_write(fd, &msg->raw.iov_len, sizeof(msg->raw.iov_len));
	if (r != sizeof(msg->header)) {
		lis_log_error("write() failed (2): r=%zd ; %d, %s", r, errno, strerror(errno));
		return LIS_ERR_IO_ERROR;
	}

	if (msg->raw.iov_len > 0) {
		r = lis_write(fd, msg->raw.iov_base, msg->raw.iov_len);
		if (r != (ssize_t)msg->raw.iov_len) {
			lis_log_error("write() failed (3): r=%zd ; %d, %s", r, errno, strerror(errno));
			return LIS_ERR_IO_ERROR;
		}
	}

	return LIS_OK;
}


enum lis_error lis_protocol_log_write(struct lis_pipes *pipes, enum lis_log_level lvl, const char *msg)
{
	size_t len;
	ssize_t r;

	len = strlen(msg);
	len = MIN(len, sizeof(pipes->log_buf) - 1);

	r = write(pipes->sorted.logs[1], &lvl, sizeof(lvl));
	if (r != sizeof(lvl)) {
		lis_log_error(
			"write() failed: r=%zd ; %d, %s",
			r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	r = write(pipes->sorted.logs[1], &len, sizeof(len));
	if (r != sizeof(len)) {
		lis_log_error(
			"write() failed (2): r=%zd ; %d, %s",
			r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	r = write(pipes->sorted.logs[1], msg, len);
	if (r != (ssize_t)len) {
		lis_log_error(
			"write() failed (3): r=%zd ; %d, %s",
			r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	return LIS_OK;
}


static enum lis_error read_log(struct lis_pipes *pipes, enum lis_log_level *lvl, const char **msg)
{
	size_t len;
	ssize_t r;

	if (pipes->sorted.logs[0] < 0) {
		// pipe has been closed on purpose
		return LIS_ERR_IO_ERROR;
	}

	r = read(pipes->sorted.logs[0], lvl, sizeof(*lvl));
	if (r != sizeof(*lvl)) {
		lis_log_error(
			"read() failed: fd=%d, r=%zd ; %d, %s",
			pipes->sorted.logs[0], r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	r = read(pipes->sorted.logs[0], &len, sizeof(len));
	if (r != sizeof(len)) {
		lis_log_error(
			"read() failed: r=%zd ; %d, %s",
			r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	r = read(pipes->sorted.logs[0], pipes->log_buf, len);
	if (r != (ssize_t)len) {
		lis_log_error(
			"read() failed: r=%zd ; %d, %s",
			r, errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}
	pipes->log_buf[len] = '\0';

	*msg = pipes->log_buf;
	return LIS_OK;
}


static enum lis_error read_stderr(struct lis_pipes *pipes, enum lis_log_level *lvl, const char **msg)
{
	ssize_t r;

	if (pipes->sorted.stderr[0] < 0) {
		// pipe has been closed on purpose
		return LIS_ERR_IO_ERROR;
	}

	*lvl = LIS_LOG_LVL_WARNING;

	r = getline(&pipes->stderr_buf, &pipes->stderr_buf_size, pipes->stderr);
	if (r < 0) {
		lis_log_error("read() failed: %d, %s", errno, strerror(errno));
		return LIS_ERR_IO_ERROR;
	}
	if (r > 0 && pipes->stderr_buf[r - 1] == '\n') {
		pipes->stderr_buf[r - 1] = '\0';
	}
	*msg = pipes->stderr_buf;
	return LIS_OK;
}


enum lis_error lis_protocol_log_read(struct lis_pipes *pipes, enum lis_log_level *lvl, const char **msg)
{
	unsigned int i;
	struct pollfd fds[] = {
		{
			.fd = pipes->sorted.logs[0],
			.events = POLLIN,
			.revents = 0,
		},
		{
			.fd = pipes->sorted.stderr[0],
			.events = POLLIN,
			.revents = 0,
		},
	};

	if (poll(fds, LIS_COUNT_OF(fds), -1 /* no timeout */) < 0) {
		lis_log_error(
			"poll() failed: %d, %s", errno, strerror(errno)
		);
		return LIS_ERR_IO_ERROR;
	}

	for (i = 0 ; i < LIS_COUNT_OF(fds) ; i++) {
		if (fds[i].revents == 0) {
			continue;
		}

		if (fds[i].revents & POLLIN) {
			if (fds[i].fd == pipes->sorted.logs[0]) {
				return read_log(pipes, lvl, msg);
			} else {
				return read_stderr(pipes, lvl, msg);
			}
		}

		if (fds[i].revents != POLLIN) {
			// normal if the other process halting was expected
			lis_log_info(
				"poll() failed on fd %u-%d: 0x%X",
				i, fds[i].fd, fds[i].revents
			);
			return LIS_ERR_IO_ERROR;
		}
	}

	lis_log_error("poll() didn't return any active file descriptor or error");
	assert(0);
	return LIS_ERR_INTERNAL_UNKNOWN_ERROR;
}



void lis_protocol_msg_free(struct lis_msg *msg)
{
	FREE(msg->raw.iov_base);
}


void lis_protocol_close(struct lis_pipes *pipes)
{
	unsigned int i;

	lis_log_info("Closing pipes ...");
	for (i = 0 ; i < LIS_COUNT_OF(pipes->all) ; i++) {
		if (pipes->all[i][0] >= 0) {
			close(pipes->all[i][0]);
		}
		if (pipes->all[i][1] >= 0) {
			close(pipes->all[i][1]);
		}
	}
	lis_log_info("Pipes closed");
}
