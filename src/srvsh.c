#include "srvsh/srvsh.h"

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <errno.h>
#include <stdarg.h>
#include <glob.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <libadt.h>

#define MAX libadt_util_max
#define MIN libadt_util_min
#define SIGNAL_RETURN_VALUE(x) (128 + (x))

#define WITH(NAME, SIZE) for (void *NAME = malloc(SIZE); NAME; free(NAME), NAME = NULL)

extern char **environ;

typedef enum {
	NO_WORK,
	SUCCESSFUL_READ,
	HANGUP,
	ERROR,
} pollfd_read_t;

/*
 * Returns the amount of space necessary for a null-terminated
 * string. Always terminates the string with a null.
 *
 * Seriously, why does C leave footguns like this all over the
 * place...?
 */
static int vslprintf(char *str, size_t size, const char *format, va_list list)
{
	int result = vsnprintf(str, size, format, list) + 1;
	int last = MIN(result, size) - 1;
	if (str && size > 0)
		str[last] = '\0';
	return result;
}

static int slprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;
	va_start(list, format);
	int result = vslprintf(str, size, format, list);
	va_end(list);
	return result;
}

static void fork_waiter(
	int (*exec)(const char *, char * const*),
	const char *path,
	char * const argv[]
)
{
	switch (fork()) {
		case -1:
			exit(1);
		case 0:
			exec(path, argv);
			exit(1);
		default: {
			int worst_exit = EXIT_SUCCESS;
			int wstatus;
			int wreturn;
			while ((wreturn = wait(&wstatus))) {
				if (wreturn < 0 && errno == ECHILD)
					exit(worst_exit);
				if (WIFEXITED(wstatus))
					worst_exit = MAX(worst_exit, WEXITSTATUS(wstatus));
				else if (WIFSIGNALED(wstatus))
					worst_exit = MAX(worst_exit, SIGNAL_RETURN_VALUE(WTERMSIG(wstatus)));
			}
			exit(worst_exit);
		}
	}
}

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

static const char *skip_words(const char *str)
{
	for (; *str; str++) {
		if (!isalnum(*str) && !ispunct(*str))
			break;
	}
	return str;
}

int get_opcode(const opcode_db *db, const char *name)
{
	char **files = (char **)db;
	const size_t len = strlen(name);
	int current = 0;

	for (; *files; files++) {
		char *value_end = *files;
		for (const char *line = *files; *line; line = next_line(value_end)) {
			if (line[0] == '#')
				continue;

			const char *name_start = skip_spaces(line);
			const char *name_end = skip_words(name_start);
			const bool selected = !strncmp(name_start, name, len);
			const bool common_prefix = selected && name_end - name_start != (ssize_t)len;

			long attempt = strtol(name_end, &value_end, 10);

			const bool error
				= attempt > INT_MAX
				|| attempt < 0;

			if (error)
				return -1;

			const bool autoval = value_end == name_end;

			if (autoval)
				current++;
			else
				current = (int)attempt;

			if (selected && !common_prefix)
				return current;
		}
	}
	return -1;
}

static char *map_path(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return NULL;

	off_t length = lseek(fd, 0, SEEK_END);
	if (length <= 0) {
		close(fd);
		return NULL;
	}

	char *raw_file = mmap(
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

opcode_db *open_opcode_db_at(const char *db_path)
{
	// haven't made up my mind whether it's better to return
	// the pattern, the globs object, or the file contents
	//
	// in reality the best solution would probably to build
	// a whole-ass hashtable or something but I'm far too
	// lazy for that
	//
	// Also this function is a perfect example of why I hate
	// returning heap-allocated memory. Whatever. Applications should
	// call this at the beginning, get the codes they need, then
	// free before doing any real work anyway.
	if (!db_path)
		return NULL;

	int pattern_length = slprintf(NULL, 0, "%s{,.d/*}", db_path);
	if (pattern_length < 0)
		return NULL;

	int success = 0;
	char **result = NULL;
	WITH(globs, sizeof(glob_t)) {
		WITH(pattern, pattern_length) {
			slprintf(pattern, pattern_length, "%s{,.d/*}", db_path);
			success = !glob(pattern, GLOB_BRACE, NULL, globs);
		}

		// frees globs and breaks out
		if (!success)
			continue;

		glob_t *glob_paths = (glob_t*)globs;
		// memory fragmentation x(
		result = calloc(
			glob_paths->gl_pathc + 1,
			sizeof(glob_t)
		);

		for (
			char
				**path = glob_paths->gl_pathv,
				**c = result;
			*path;
			path++, c++
		) {
			*c = map_path(*path);
		}
	}
	return result;
}

opcode_db *open_opcode_db(void)
{
	return open_opcode_db_at(getenv("OPCODE_DATABASE"));
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
	char **files = (char**)db;
	if (files)
		for (; *files; files++) {
			munmap(*files, strlen(*files));
		}
	free(db);
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

/*
 * Common poll code between different pollop functions
 *
 * I really don't like this function but the recvmsg interface
 * kinda forces my hand here
 */
static pollfd_read_t process_pollfd(struct pollfd *fd, pollop_callback *callback, void *context)
{
	if (fd->revents & POLLIN) {
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

		ssize_t received = recvmsg(fd->fd, &hdr, 0);
		if (received < 0) {
			return ERROR;
		}

		if (received == 0) {
			return HANGUP;
		}

		if (header.size == 0) {
			callback(
				fd->fd,
				header.opcode,
				NULL,
				0,
				hdr,
				context
			);
			return SUCCESSFUL_READ;
		}

		void *attempt = malloc(header.size);
		if (!attempt) {
			return ERROR;
		}

		struct iovec newbuf = {
			.iov_base = attempt,
			.iov_len = header.size,
		};

		struct msghdr bighdr = {
			.msg_iov = &newbuf,
			.msg_iovlen = 1,
		};

		received = recvmsg(fd->fd, &bighdr, 0);
		if (received < 0) {
			free(attempt);
			return ERROR;
		}

		callback(
			fd->fd,
			header.opcode,
			attempt,
			header.size,
			hdr,
			context
		);

		free(attempt);

		return SUCCESSFUL_READ;
	} else if (fd->revents & POLLHUP) {
		return HANGUP;
	} else if (
		fd->revents & POLLNVAL
		|| fd->revents & POLLERR
	) {
		return ERROR;
	}
	return NO_WORK;
}

struct pollfd pollopfds(
	struct pollfd *fds,
	int count,
	pollop_callback *callback,
	void *context,
	int timeout
)
{
	static const struct pollfd err = {.fd = -1};

	fds->events = POLLIN;

	if (count < 0)
		return err;

	int changed = poll(fds, count, timeout);

	if (changed < 0)
		return err;

	if (changed == 0)
		return (struct pollfd) { 0 };

	struct pollfd *fd = fds;
	for (; changed > 0 && fd < &fds[count]; fd++) {
		pollfd_read_t result = process_pollfd(fd, callback, context);
		if (result == ERROR)
			return err;
		else if (result == HANGUP) {
			fd->revents &= ~POLLIN;
			fd->revents |= POLLHUP;
			fd->fd = ~fd->fd;
			return (struct pollfd) {
				.fd = ~fd->fd,
				.events = fd->events,
				.revents = fd->revents,
			};
		}
		else if (result == SUCCESSFUL_READ)
			changed--;
	}

	return *(fd - 1);
}

struct pollfd pollopfd(
	struct pollfd fd,
	pollop_callback *callback,
	void *context,
	int timeout
)
{
	return pollopfds(&fd, 1, callback, context, timeout);
}

struct pollfd pollopsrv(
	pollop_callback *callback,
	void *context,
	int timeout
)
{
	struct pollfd fd = {.fd = SRV_FILENO};
	return pollopfd(fd, callback, context, timeout);
}

struct pollfd pollop(
	pollop_callback *callback,
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

	return pollopfds(fds, total, callback, context, timeout);
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

static int get_clients_end()
{
	// probably a better way to do this
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	close(fd);
	return fd;
}

static struct clistate exec_impl(
	bool does_lookup,
	const char *path,
	char *const argv[],
	char *const envp[],
	bool (*cli_spawner)(void *context),
	void *context
)
{
	static const struct clistate error = { -1, -1 };
	struct clistate result = error;
	int (*const exec)(const char*, char *const[])
		= does_lookup ? execvp : execv;

	int sockets[2] = { -1, -1 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return error;

	result.pid = fork();
	switch (result.pid) {
		case -1:
			return error;
		case 0: {
			for (int sock = sockets[0]; sock > SRV_FILENO; sock--)
				close(sock);
			if (dup2(sockets[1], SRV_FILENO) < 0)
				exit(1);
			close(sockets[1]);

			if (cli_spawner)
				if (!cli_spawner(context))
					exit(1);

			int clients_end = get_clients_end();
			if (clients_end < 0)
				exit(1);

			// 22 characters should be enough for a
			// 64bit int + sign + null byte
			// but if we end up with that many clients 
			// or file descriptors I have other concerns
			typedef char int64_str[22];
			int64_str clients_end_str = { 0 };
			if (
				snprintf(
					clients_end_str,
					sizeof(clients_end_str),
					"%d",
					clients_end
				) < 0
			) {
				exit(1);
			}

			environ = NULL;
			for (char * const* env = envp; *env; env++) {
				if (putenv(*env))
					exit(1);
			}

			const bool overwrite = true;
			if (
				setenv(
					"SRVSH_CLIENTS_END",
					clients_end_str,
					overwrite
				) < 0
			) {
				exit(1);
			}

			if (cli_spawner) {
				fork_waiter(exec, path, argv);
				// the fork_waiter already calls exit() but this
				// shuts the compiler up
				exit(1);
			} else {
				exec(path, argv);
				exit(1);
			}
		}
		default:
			close(sockets[1]);
			result.socket = sockets[0];
			return result;
	}
}

static struct clistate execl_impl(
	bool has_envp,
	bool does_lookup,
	const char *path,
	const char *arg0,
	va_list args,
	bool (*cli_spawner)(void *),
	void *context
)
{
	va_list args_for_length = { 0 };
	int arglength = 1;

	va_copy(args_for_length, args);
	const char *arg = NULL;

	while ((arg = va_arg(args_for_length, const char*)))
		arglength++;

	// +1 for the null terminator
	char **argv = calloc(arglength + 1, sizeof(char**));
	memcpy(argv, &arg0, sizeof(*argv));
	for (char **cur = argv; arglength; cur++, arglength--)
		*cur = va_arg(args, char*);

	// skip the null arg
	va_arg(args, const char*);

	char *const *envp = has_envp ?
		va_arg(args, char**) :
		environ;

	struct clistate result = exec_impl(
		does_lookup,
		path,
		argv,
		envp,
		cli_spawner,
		context
	);

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
	const bool
		has_env = false,
		does_lookup = false;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		NULL,
		NULL
	);
	va_end(args);
	return result;
}

struct clistate cliexecle(const char *path, const char *arg0, ...)
{
	va_list args = { 0 };
	va_start(args, arg0);
	const bool
		has_env = true,
		does_lookup = false;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		NULL,
		NULL
	);
	va_end(args);
	return result;
}

struct clistate cliexeclp(const char *path, const char *arg0, ...)
{
	va_list args = { 0 };
	va_start(args, arg0);
	const bool
		has_env = true,
		does_lookup = true;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		NULL,
		NULL
	);
	va_end(args);
	return result;
}

struct clistate cliexecv(const char *path, char *const argv[])
{
	return cliexecve(path, argv, environ);
}

struct clistate cliexecve(const char *path, char *const argv[], char *const envp[])
{
	return srvexecve(
		NULL,
		NULL,
		path,
		argv,
		envp
	);
}

struct clistate cliexecvp(const char *command, char *const argv[])
{
	return cliexecvpe(command, argv, environ);
}

struct clistate cliexecvpe(const char *path, char *const argv[], char *const envp[])
{
	return srvexecvpe(
		NULL,
		NULL,
		path,
		argv,
		envp
	);
}

struct clistate srvexecl(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	const char *arg0,
	...
)
{
	va_list args = { 0 };
	va_start(args, arg0);
	const bool
		has_env = false,
		does_lookup = false;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		cli_spawner,
		context
	);
	va_end(args);
	return result;
}

struct clistate srvexecle(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	const char *arg0,
	...
)
{
	va_list args = { 0 };
	va_start(args, arg0);
	const bool
		has_env = true,
		does_lookup = false;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		cli_spawner,
		context
	);
	va_end(args);
	return result;
}

struct clistate srvexeclp(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	const char *arg0,
	...
)
{
	va_list args = { 0 };
	va_start(args, arg0);
	const bool
		has_env = true,
		does_lookup = true;
	struct clistate result = execl_impl(
		has_env,
		does_lookup,
		path,
		arg0,
		args,
		cli_spawner,
		context
	);
	va_end(args);
	return result;
}

struct clistate srvexecve(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	char *const argv[],
	char *const envp[]
)
{
	return exec_impl(
		false,
		path,
		argv,
		envp,
		cli_spawner,
		context
	);
}

struct clistate srvexecv(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	char *const argv[]
)
{
	return srvexecve(
		cli_spawner,
		context,
		path,
		argv,
		environ
	);
}

struct clistate srvexecvpe(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *path,
	char *const argv[],
	char *const envp[]
)
{
	return exec_impl(
		true,
		path,
		argv,
		envp,
		cli_spawner,
		context
	);
}

struct clistate srvexecvp(
	bool (*cli_spawner)(void *context),
	void *context,
	const char *command,
	char *const argv[]
)
{
	return srvexecvpe(
		cli_spawner,
		context,
		command,
		argv,
		environ
	);
}

