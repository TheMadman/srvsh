#include "srvsh/srvsh.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

int cli_end(void)
{
	static int _cli_end = 0;
	if (!_cli_end)
		_cli_end = atoi(getenv("SRVSH_CLIENTS_END"));
	return _cli_end;
}

void *open_opcode_database(void)
{
	static const char *db_path = NULL;

	if (!db_path)
		db_path = getenv("SRVSH_DATABASE");
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

	void *raw_file = mmap(
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
