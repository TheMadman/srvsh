#include "srvsh/srvsh.h"
#include "srvsh/parse.h"

#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <locale.h>
#include <wait.h>
#include <errno.h>

#include <libadt/lptr.h>
#include <libadt/util.h>
#include <scallop-lang/lex.h>

// gettext placeholder
#define _(str) str
#define SIGNAL_RETURN_VALUE(sig) (128 + sig)
#define perror_exit(str) perror(str), exit(EXIT_FAILURE)

#define MAX libadt_util_max

typedef struct scallop_lang_token token_t;
typedef struct libadt_lptr lptr_t;
typedef struct libadt_const_lptr const_lptr_t;
typedef struct parse_statement_command command_t;
typedef struct parse_statement statement_t;

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	if (argc < 2) {
		fprintf(stderr, _("Usage: %s <script-file>\n"), basename(argv[0]));
		return EXIT_FAILURE;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		perror_exit(_("Failed to open file"));

	off_t length = lseek(fd, 0, SEEK_END);

	void *raw_file = mmap(
		NULL,
		(size_t)length,
		PROT_READ,
		MAP_PRIVATE,
		fd,
		0
	);

	if (!raw_file)
		perror_exit(_("Failed to map file"));

	close(fd);

	const_lptr_t file = {
		.buffer = raw_file,
		.size = 1,
		.length = (ssize_t)length,
	};

	if (srvsh_parse_script(file) < 0) {
		fprintf(stderr, "%s\n", _("Error parsing script"));
		exit(EXIT_FAILURE);
	}

	int worst_exit = EXIT_SUCCESS;
	int wstatus;
	int wreturn;
	while ((wreturn = wait(&wstatus))) {
		if (wreturn < 0 && errno == ECHILD)
			exit(worst_exit);
		if (wreturn < 0)
			perror_exit(_("Waiting for children failed"));
		if (WIFEXITED(wstatus))
			worst_exit = MAX(worst_exit, WEXITSTATUS(wstatus));
		else if (WIFSIGNALED(wstatus))
			worst_exit = MAX(worst_exit, SIGNAL_RETURN_VALUE(WTERMSIG(wstatus)));
	}
	return worst_exit;

}
