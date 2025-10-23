/*
 * srvsh - A server/client shell script interpreter
 * Copyright (C) 2024  Marcus Harrison
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "srvsh.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int
	server = SRV_FILENO,
	client = 4;

void test_cli_end(void)
{
	assert(cli_end() == 5);
}

void test_cli_count(void)
{
	assert(cli_count() == 1);
}

void test_writesrv(void)
{
	int payload = 1;
	ssize_t result = writesrv(1, &payload, sizeof(payload));
	assert(result == sizeof(struct srvsh_header) + sizeof(payload));

	struct {
		struct srvsh_header header;
		int payload;
	} buffer = { 0 };
	assert(read(client, &buffer, sizeof(buffer)) == result);
	assert(buffer.header.opcode == 1);
	assert(buffer.header.size == sizeof(int));
	assert(buffer.payload == 1);
}

void test_writeop(void)
{
	int payload = 2;
	ssize_t result = writeop(client, 2, &payload, sizeof(payload));
	assert(result == sizeof(struct srvsh_header) + sizeof(payload));

	struct {
		struct srvsh_header header;
		int payload;
	} buffer = { 0 };
	assert(read(server, &buffer, sizeof(buffer)) == result);
	assert(buffer.header.opcode == 2);
	assert(buffer.header.size == sizeof(payload));
	assert(buffer.payload == 2);
}

void test_sendmsgop(void)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	{
		assert(sock >= 0);

		union {
			char buf[CMSG_SPACE(sizeof(sock))];
			struct cmsghdr align;
		} buf = { 0 };
		struct cmsghdr *cmsg = &buf.align;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(sock));
		memcpy(CMSG_DATA(cmsg), &sock, sizeof(sock));

		ssize_t result = sendmsgop(server, 3, NULL, 0, cmsg, sizeof(buf));

		assert(result == sizeof(struct srvsh_header));
	}

	{
		// The data sent on the socket
		struct srvsh_header data = { 0 };

		// The rest of this is to use recvmsg,
		// because we expect some control data
		struct iovec iov = {
			.iov_base = &data,
			.iov_len = sizeof(data),
		};
		union {
			char buf[CMSG_SPACE(sizeof(int))];
			struct cmsghdr align;
		} buf = { 0 };

		struct msghdr msghdr = {
			.msg_iov = &iov,
			.msg_iovlen = 1,
			.msg_control = &buf,
			.msg_controllen = sizeof(buf),
		};

		ssize_t result = recvmsg(client, &msghdr, 0);

		assert(result == sizeof(data));
		assert(data.opcode == 3);
		assert(data.size == 0);

		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);
		assert(cmsg);
		assert(cmsg->cmsg_type == SCM_RIGHTS);
		assert(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));

		int new_fd = -1;
		memcpy(&new_fd, CMSG_DATA(cmsg), sizeof(int));
		assert(close(new_fd) == 0);
	}

	close(sock);
}

bool callback_run = false;

void test_pollop_callback(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	callback_run = true;
	assert(fd == client);
	assert(opcode == 5);
	assert(size == sizeof(int));
	assert(*(int*)data == 6);

	assert(context == &client);
}


void test_pollop(void)
{
	int data = 6;
	writesrv(5, &data, sizeof(data));
	pollop(test_pollop_callback, &client, -1);
	assert(callback_run == true);
}

int main()
{
	if (setenv("SRVSH_CLIENTS_END", "5", 1) < 0)
		return 1;

	int sockets[2] = { 0 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return 1;

	// ctest keeps a log fd open before running the tests for
	// some reason
	//
	// Don't know what ctest is trying to do, but I'm trying
	// to test so I don't care
	if (sockets[0] != server) {
		if (dup2(sockets[0], SRV_FILENO) < 0)
			return 1;
		close(sockets[0]);
	}

	if (sockets[1] != client) {
		if (dup2(sockets[1], 4) < 0)
			return 1;
		close(sockets[1]);
	}

	test_cli_end();
	test_cli_count();
	test_writesrv();
	test_writeop();
	test_sendmsgop();
	test_pollop();
}
