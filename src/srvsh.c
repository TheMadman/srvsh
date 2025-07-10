#include "srvsh/srvsh.h"

#include <stdlib.h>

int cli_end()
{
	static int _cli_end = 0;
	if (!_cli_end)
		_cli_end = atoi(getenv("SRVSH_CLIENTS_END"));
	return _cli_end;
}
