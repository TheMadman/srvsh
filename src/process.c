#include "srvsh/process.h"

#include "srvsh/srvsh.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libadt/util.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h> // perror
#include <errno.h>

#define clptr_raw libadt_const_lptr_raw
#define arrlength libadt_util_arrlength
#define arrend libadt_util_arrend

#define _STR(PARAM) #PARAM
#define STR(PARAM) _STR(PARAM)

// temp gettext wrapper
#define _(str) str

typedef struct libadt_const_lptr clptr;

int fork_wrapper(
	clptr statement,
	int srv,
	int cli
)
{
	int pid = fork();
	switch (pid) {
		case -1:
			return -1;
		case 0: {
			struct sockaddr_un addr = { 0 };
			socklen_t addrlen = sizeof(addr);
			// If we have no server, that's fine -
			// it's up to the command to be able to
			// run as a top-level command, or break
			// if it can't
			if (getpeername(srv, (struct sockaddr*)&addr, &addrlen) == 0) {
				close(srv);

				srv = socket(AF_UNIX, SOCK_STREAM, 0);
				if (srv < 0) {
					perror(_("Failed to recreate srv socket"));
					exit(EXIT_FAILURE);
				}
				if (connect(srv, (struct sockaddr*)&addr, addrlen) < 0) {
					perror(_("Failed to connect new srv socket"));
					exit(EXIT_FAILURE);
				}

				if (dup2(srv, SRV_FILENO) == -1) {
					perror(_("Failed to assign SRV_FILENO"));
					exit(EXIT_FAILURE);
				}
			}

			// Similarly, if we get here and there's no
			// clients, that's also fine. Commands that
			// expect clients but have no client fd should
			// just break
			dup2(cli, CLI_FILENO);

			exec_command(statement);
			// we only get here if execvp() errors
			const char *command = clptr_raw(*(clptr*)clptr_raw(statement));
			fprintf(stderr, _("Failed to execute %s: %s\n"), command, strerror(errno));
			exit(EXIT_FAILURE);
		}
		default: {
			// do we need anything else here?
			return pid;
		}
	}
}

int exec_command(clptr statement)
{
	// Maybe I should put this logic in libadt somewhere
	char **args = calloc((size_t)statement.length + 1, sizeof(char*));
	for (
		char **out = args;
		libadt_const_lptr_in_bounds(statement);
		statement = libadt_const_lptr_index(statement, 1),
		out++
	) {
		const clptr
			*current = clptr_raw(statement);
		*out = strndup(clptr_raw(*current), (size_t)current->length);
	}

	// I don't care if we leak memory if execvp() fails
	return execvp(*args, args);
}
