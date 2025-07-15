#include "srvsh/srvsh.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>

int cli_end(void)
{
	static int _cli_end = 0;
	if (!_cli_end) {
		const char *envvar = getenv("SRVSH_CLIENTS_END");
		if (!envvar)
			return -1;
		_cli_end = atoi(envvar);
	}
	return _cli_end;
}

static const char *next_line(const char *str)
{
	for (; *str; str++) {
		if (*str == '\n') {
			return ++str;
		}
	}
	return str;
}

static const char *skip_spaces(const char *str)
{
	for (; *str; str++) {
		if (!isspace(*str))
			break;
	}
	return str;
}

int get_opcode(const opcode_db *db, const char *name)
{
	const size_t len = strlen(name);
	for (const char *line = db; *line; line = next_line(line)) {
		if (line[0] == '#')
			continue;

		if (strncmp(line, name, len))
			continue;

		const char *value_start = skip_spaces(&line[len]);
		char *value_end = NULL;

		long attempt = strtol(value_start, &value_end, 10);

		const bool error
			= value_end == value_start
			|| attempt > INT_MAX
			|| attempt < 0;

		if (error)
			return -1;

		return (int)attempt;
	}
	return -1;
}

opcode_db *open_opcode_db(void)
{
	const char *db_path = getenv("OPCODE_DATABASE");
	if (!db_path)
		return NULL;
	int fd = open(db_path, O_RDONLY);
	if (fd < 0)
		return NULL;

	off_t length = lseek(fd, 0, SEEK_END);
	if (length <= 0) {
		close(fd);
		return NULL;
	}

	opcode_db *raw_file = mmap(
		NULL,
		(size_t)length,
		PROT_READ,
		MAP_PRIVATE,
		fd,
		0
	);
	close(fd);
	return raw_file;
}

ssize_t writesrv(int opcode, void *buf, size_t len)
{
	return writeop_r(SRV_FILENO, opcode, buf, len);
}

ssize_t writeop_r(
	int fd,
	int opcode,
	void *buf,
	int len
)
{
	return sendmsgop_r(
		fd,
		opcode,
		buf,
		len,
		NULL,
		0
	);
}

ssize_t sendmsgop_r(
	int fd,
	int opcode,
	void *buf,
	int len,
	struct cmsghdr *cmsg,
	size_t cmsg_len
)
{
	if (len < 0)
		return -1;

	struct srvsh_header hd = {
		.opcode = opcode,
		.length = len,
	};

	struct iovec inputs[] = {
		{
			.iov_base = &hd,
			.iov_len = sizeof(hd),
		},
		{
			.iov_base = buf,
			.iov_len = len,
		},
	};
	struct msghdr msg = {
		.msg_iov = inputs,
		.msg_iovlen = 2,
		.msg_control = cmsg,
		.msg_controllen = cmsg_len,
	};
	return sendmsg(fd, &msg, 0);
}

void close_opcode_db(opcode_db *db)
{
	/*
	 * TODO: check if the file is null-terminated
	 * plaintext in open_opcode_db()
	 */
	if (db)
		munmap(db, strlen(db));
}
