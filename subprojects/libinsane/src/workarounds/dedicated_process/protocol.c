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


static enum lis_error lis_read(int fd, void *buf, size_t count)
{
	ssize_t r;
	size_t total = 0;

	do {
		r = read(fd, buf, count - total);
		if (r <= 0) {
			lis_log_error(
				"read() failed: fd=%d, r=%zd, got=%zd, expected=%zd; %d, %s",
				fd, r, count, total, errno, strerror(errno)
			);
			return LIS_ERR_IO_ERROR;
		}
		buf += r;
		total += r;
	} while(total < count);

#ifdef PROTOCOL_DEBUG
	lis_hexdump("read", buf, r);
#endif
	return LIS_OK;
}


static enum lis_error lis_write(int fd, const void *buf, size_t count)
{
	ssize_t w;
	size_t total = 0;

	do {
		w = write(fd, buf, count - total);
		if (w <= 0) {
			lis_log_error(
				"write() failed: fd=%d, w=%zd, written=%zd, expected=%zd; %d, %s",
				fd, w, count, total, errno, strerror(errno)
			);
			return LIS_ERR_IO_ERROR;
		}
		buf += w;
		total += w;
	} while(total < count);

#ifdef PROTOCOL_DEBUG
	lis_hexdump("write", buf, w);
#endif
	return LIS_OK;
}


enum lis_error lis_protocol_msg_read(int fd, struct lis_msg *msg)
{
	enum lis_error err;

	memset(msg, 0, sizeof(*msg));

	err = lis_read(fd, &msg->header, sizeof(msg->header));
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	if (LIS_IS_ERROR(msg->header.err)) {
		return msg->header.err;
	}

	err = lis_read(fd, &msg->raw.iov_len, sizeof(msg->raw.iov_len));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (msg->raw.iov_len > 0) {
		msg->raw.iov_base = malloc(msg->raw.iov_len);
		if (msg->raw.iov_base == NULL) {
			lis_log_error(
				"Out of memory (requested: %zu)\n", msg->raw.iov_len
			);
			return LIS_ERR_NO_MEM;
		}

		err = lis_read(fd, msg->raw.iov_base, msg->raw.iov_len);
		if (LIS_IS_ERROR(err)) {
			return err;
		}
	}

	return LIS_OK;
}

enum lis_error lis_protocol_msg_write(int fd, const struct lis_msg *msg)
{
	enum lis_error err;

	err = lis_write(fd, &msg->header, sizeof(msg->header));
	if (LIS_IS_ERROR(err)) {
		return err;
	}
	if (LIS_IS_ERROR(msg->header.err)) {
		return LIS_OK;
	}

	err = lis_write(fd, &msg->raw.iov_len, sizeof(msg->raw.iov_len));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	if (msg->raw.iov_len > 0) {
		err = lis_write(fd, msg->raw.iov_base, msg->raw.iov_len);
		if (LIS_IS_ERROR(err)) {
			return err;
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
	enum lis_error err;

	if (pipes->sorted.logs[0] < 0) {
		// pipe has been closed on purpose
		return LIS_ERR_IO_ERROR;
	}

	err = lis_read(pipes->sorted.logs[0], lvl, sizeof(*lvl));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	err = lis_read(pipes->sorted.logs[0], &len, sizeof(len));
	if (LIS_IS_ERROR(err)) {
		return err;
	}

	err = lis_read(pipes->sorted.logs[0], pipes->log_buf, len);
	if (LIS_IS_ERROR(err)) {
		return err;
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
