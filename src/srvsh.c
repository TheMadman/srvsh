#include "srvsh/srvsh.h"

#include <stdlib.h>

int cli_end(void)
{
	static int _cli_end = 0;
	if (!_cli_end)
		_cli_end = atoi(getenv("SRVSH_CLIENTS_END"));
	return _cli_end;
}

int open_opcode_database(void)
{
	static const char *db_path = NULL;

	if (!db_path)
		db_path = getenv("SRVSH_DATABASE");
	if (!db_path)
		return -1;
	return open(db_path, O_RDONLY);
}
