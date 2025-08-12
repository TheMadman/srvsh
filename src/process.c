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

typedef struct libadt_const_lptr clptr;

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
