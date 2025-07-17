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

#ifndef SRVSH_SRVSH
#define SRVSH_SRVSH

#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 */

/**
 * \brief Constant file descriptor for the Server.
 *
 * This is set up by the srvsh to point to server, if one
 * exists.
 */
#define SRV_FILENO 3

/**
 * \brief Constant value for the lowest file descriptor a client
 * 	can be.
 *
 * Note that a program may have no clients, in which case, the
 * cli_end() function will return this value.
 */
#define CLI_BEGIN 4

typedef void opcode_db;

/**
 * \brief Returns one past the last client file descriptor.
 *
 * All client file descriptors can be accessed with
 * for(int i = CLI_BEGIN; i < cli_end(); i++).
 *
 * If there was an error discovering the end of the clients,
 * -1 is returned.
 */
int cli_end(void);

/**
 * \brief Returns a count of how many clients are connected.
 *
 * If there was an error discovering the number of clients,
 * a negative number is returned.
 */
int cli_count(void);

/**
 * \brief A header prefix for IPC between servers/clients.
 */
struct srvsh_header {
	int opcode;
	int length;
};

/**
 * \brief Writes a message on the SRV_FILENO with the specified type,
 * 	sending the specified buffer with the given length.
 *
 * \sa writeop()
 */
ssize_t writesrv(int opcode, void *buf, size_t len);

/**
 * \brief Writes a message to the file descriptor given in fd with
 * 	the specified type, sending the specified buffer with the
 * 	given length.
 */
ssize_t writeop(
	int fd,
	int opcode,
	void *buf,
	int len
);

/**
 * \brief Writes a message to the file descriptor given in fd,
 * 	in the same manner as writefd, except it also supports
 * 	ancillary data.
 */
ssize_t sendmsgop(
	int fd,
	int opcode,
	void *buf,
	int len,
	struct cmsghdr *cmsg,
	size_t cmsg_len
);

/**
 * \brief Returns a pointer to the opcode database
 * 	defined by the environment variable OPCODE_DATABASE.
 *
 * \returns NULL if OPCODE_DATABASE is not set, or the path cannot
 * 	be opened for reading. Returns a pointer to the database
 * 	otherwise.
 */
opcode_db *open_opcode_db(void);

/**
 * \brief Release an opcode database opened with
 * 	open_opcode_db().
 *
 * \param db A database previously opened with open_opcode_db().
 */
void close_opcode_db(opcode_db *db);

/**
 * \brief Queries the database given in db for the opcode with
 * 	the given name.
 *
 * \param db The database to query.
 * \param name The name to query.
 *
 * \returns A positive integer on match, or -1 on failure.
 */
int get_opcode(const opcode_db *db, const char *name);

/**
 * \brief Initializes an array of struct pollfd for
 * 	the server and all currently-connected clients.
 *
 * The pollfd structs are already initialized for the
 * POLLIN event for convenience.
 *
 * \param buffer The buffer to write the pollfds into.
 * \param buflen The number of pollfd structs the buffer
 * 	can hold.
 *
 * \returns The number of pollfds initialized, or -1
 * 	if an error was encountered.
 */
int srvcli_polls(struct pollfd *fds, int buflen);

/**
 * \brief Initializes an array of struct pollfd for
 * 	all currently-connected clients.
 *
 * The pollfd structs are already initialized for the
 * POLLIN event for convenience.
 *
 * \param buffer The buffer to write the pollfds into.
 * \param buflen The number of pollfd structs the buffer
 * 	can hold.
 *
 * \returns The number of pollfds initialized, or -1
 * 	if an error was encountered.
 */
int cli_polls(struct pollfd *buffer, int buflen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SRVSH_SRVSH
