#include "srvsh/srvsh.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

extern char **environ;

int cli_end(void)
{
	static int _cli_end = 0;
	if (!_cli_end) {
		const char *envvar = getenv("SRVSH_CLIENTS_END");
		if (!envvar)
			_cli_end = CLI_BEGIN;
		else
			_cli_end = atoi(envvar);
	}
	return _cli_end;
}

int cli_count(void)
{
	return cli_end() - CLI_BEGIN;
}

bool is_cli(int fd)
{
	return CLI_BEGIN <= fd
		&& fd < cli_end();
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
		const bool common_prefix = value_start == &line[len];
		if (common_prefix) {
			// The message name is a prefix for another
			// opcode, skip 'till end of line
			continue;
		}

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

ssize_t writesrv(int opcode, const void *buf, size_t len)
{
	return writeop(SRV_FILENO, opcode, buf, len);
}

ssize_t writeop(
	int fd,
	int opcode,
	const void *buf,
	int len
)
{
	return sendmsgop(
		fd,
		opcode,
		buf,
		len,
		NULL,
		0
	);
}

ssize_t sendmsgop(
	int fd,
	int opcode,
	const void *buf,
	int len,
	void *cmsg,
	size_t cmsg_len
)
{
	if (len < 0)
		return -1;

	struct srvsh_header hd = {
		.opcode = opcode,
		.size = len,
	};

	struct iovec inputs[] = {
		{
			.iov_base = &hd,
			.iov_len = sizeof(hd),
		},
		{
			.iov_base = (void*)buf,
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

int srvcli_polls(struct pollfd *fds, int buflen)
{
	int count = cli_count() + 1;
	if (buflen < count)
		return -1;

	*fds = (struct pollfd) {
		.fd = SRV_FILENO,
		.events = POLLIN,
	};

	return cli_polls(fds + 1, buflen - 1) + 1;
}

int cli_polls(struct pollfd *fds, int buflen)
{
	int count = cli_count();
	if (count < 0)
		return -1;

	if (buflen < count)
		return -1;

	struct pollfd *current = fds;
	int end = cli_end();
	for (int cli = CLI_BEGIN; cli < end; cli++) {
		*current = (struct pollfd) {
			.fd = cli,
			.events = POLLIN,
		};
		current++;
	}
	return count;
}

struct pollfd pollop(
	void (*callback)(
		int fd,
		int opcode,
		void *buf,
		int len,
		struct msghdr header,
		void *context
	),
	void *context,
	int timeout
)
{
	static const struct pollfd err = {.fd = -1};
	static struct pollfd *fds = NULL;

	int total = cli_count() + 1;
	if (!fds) {
		fds = calloc(total, sizeof(*fds));
		if (!fds)
			return err;

		srvcli_polls(fds, total);
	}

	int changed = poll(fds, total, timeout);

	if (changed < 0) {
		return err;
	}

	struct pollfd *current = fds;
	for (; changed > 0 && current < &fds[total]; current++) {
		if (current->revents & POLLIN) {
			// TODO: don't like pretty much any of this
			struct srvsh_header header = { 0 };
			char cmsg_buf[1024] = { 0 };

			struct iovec buf = {
				.iov_base = &header,
				.iov_len = sizeof(header),
			};
			struct msghdr hdr = {
				.msg_iov = &buf,
				.msg_iovlen = 1,
				.msg_control = cmsg_buf,
				.msg_controllen = sizeof(cmsg_buf),
			};

			ssize_t received = recvmsg(current->fd, &hdr, 0);
			if (received < 0) {
				return err;
			}

			if (received == 0) {
				// I don't know why this happens but
				// I don't want my consumers to be as confused
				// as I am
				current->revents &= ~POLLIN;

				current->fd = ~current->fd;
				return (struct pollfd) {
					.fd = ~current->fd,
					.events = current->events,
					.revents = current->revents,
				};
			}

			if (header.size == 0) {
				callback(
					current->fd,
					header.opcode,
					NULL,
					0,
					hdr,
					context
				);
				continue;
			}

			void *attempt = malloc(header.size);
			if (!attempt) {
				return err;
			}

			struct iovec newbuf = {
				.iov_base = attempt,
				.iov_len = header.size,
			};

			struct msghdr bighdr = {
				.msg_iov = &newbuf,
				.msg_iovlen = 1,
			};

			received = recvmsg(current->fd, &bighdr, 0);
			if (received < 0) {
				free(attempt);
				return err;
			}

			callback(
				current->fd,
				header.opcode,
				attempt,
				header.size,
				hdr,
				context
			);

			free(attempt);

			changed--;
		} else if (
			current->revents & POLLHUP
			|| current->revents & POLLNVAL
			|| current->revents & POLLERR
		) {
			// ignore on future uses
			current->fd = ~current->fd;

			// return the real FD with issues
			return (struct pollfd) {
				.fd = ~current->fd,
				.events = current->events,
				.revents = current->revents,
			};
		}
	}
	return *(current - 1);
}

void close_cmsg_fds(struct msghdr header)
{
	if (!header.msg_control)
		return;

	struct cmsghdr *chdr = CMSG_FIRSTHDR(&header);
	do {
		if (
			chdr->cmsg_level != SOL_SOCKET
			|| chdr->cmsg_type != SCM_RIGHTS
		)
			continue;

		int *fds = malloc(chdr->cmsg_len);
		if (!fds)
			return;

		memcpy(fds, CMSG_DATA(chdr), chdr->cmsg_len);

		const size_t arrlength = chdr->cmsg_len / sizeof(int);
		for (size_t i = 0; i < arrlength; i++) {
			close(fds[i]);
		}

		free(fds);
	} while ((chdr = CMSG_NXTHDR(&header, chdr)));
}

static struct clistate execl_impl(
	bool has_envp,
	const char *path,
	va_list args
)
{
	va_list args_for_length = { 0 };
	int arglength = 0;

	va_copy(args_for_length, args);
	const char *arg = NULL;

	while ((arg = va_arg(args_for_length, const char*)))
		arglength++;

	// +1 for the null terminator
	char **argv = calloc(arglength + 1, sizeof(char**));
	for (char **cur = argv; arglength; cur++, arglength--)
		*cur = va_arg(args, char*);

	// skip the null arg
	va_arg(args, const char*);

	char *const *envp = has_envp ?
		va_arg(args, char**) :
		environ;

	struct clistate result = cliexecve(path, argv, envp);

	// could use a context-manager-like interface here but
	// this function is (currently) tiny
	free(argv);
	va_end(args_for_length);

	return result;
}

struct clistate cliexecl(const char *path, const char *arg0, ...)
{
	va_list args = { 0 };
	va_start(args, arg0);
	struct clistate result = execl_impl(false, path, args);
	va_end(args);
	return result;
}

struct clistate cliexecle(const char *path, const char *arg0, ...)
{
	va_list args = { 0 };
	va_start(args, arg0);
	struct clistate result = execl_impl(true, path, args);
	va_end(args);
	return result;
}

struct clistate cliexecv(const char *path, char *const argv[])
{
	return cliexecve(path, argv, environ);
}

struct clistate cliexecve(const char *path, char *const argv[], char *const envp[])
{
	static const struct clistate error = { -1, -1 };
	struct clistate result = error;

	int sockets[2] = { -1, -1 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return error;

	result.pid = fork();
	switch(result.pid) {
		case -1:
			return error;

		case 0:
			for (int sock = sockets[0]; sock > SRV_FILENO; sock--)
				close(sock);
			if (dup2(sockets[1], SRV_FILENO) < 0)
				exit(1);
			close(sockets[1]);
			execve(path, argv, envp);
			exit(1);
		default:
			close(sockets[1]);
			result.socket = sockets[0];
			return result;
	}
}
