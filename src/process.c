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

// temp gettext wrapper
#define _(str) str

typedef struct libadt_const_lptr clptr;

int fork_wrapper(
	clptr statement,
	int cli
)
{
	int pid = fork();
	switch (pid) {
		case -1:
			return -1;
		case 0: {
			dup2(cli, SRV_FILENO);
			close(cli);

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
