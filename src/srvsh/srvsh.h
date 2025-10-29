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
#include <stdbool.h>

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
 * If there are no clients, CLI_BEGIN is returned.
 *
 * Note that this function does not consider new clients spawned
 * with the cliexec*() functions, only those spawned before
 * the program started.
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
 * \returns True if the given file descriptor is a client,
 * 	false otherwise.
 */
bool is_cli(int fd);

/**
 * \brief A header prefix for IPC between servers/clients.
 */
struct srvsh_header {
	int opcode;
	int size;
};

/**
 * \brief Writes a message on the SRV_FILENO with the specified type,
 * 	sending the specified buffer with the given length.
 *
 * \sa writeop()
 */
ssize_t writesrv(int opcode, const void *buf, size_t len);

/**
 * \brief Writes a message to the file descriptor given in fd with
 * 	the specified type, sending the specified buffer with the
 * 	given length.
 */
ssize_t writeop(
	int fd,
	int opcode,
	const void *buf,
	int len
);

/**
 * \brief Writes a message to the file descriptor given in fd,
 * 	in the same manner as writefd, except it also supports
 * 	ancillary data.
 *
 * See sendmsg(2) and cmsg(3) for information on the cmsg
 * and cmsg_len parameters.
 */
ssize_t sendmsgop(
	int fd,
	int opcode,
	const void *buf,
	int len,
	void *cmsg,
	size_t cmsg_len
);

/**
 * \brief A type defining the callback type
 * 	used by pollop* functions.
 *
 * The callback takes the following arguments:
 * 	- fd - The file descriptor this read comes from
 * 	- opcode - The opcode sent with the message
 * 	- buf - A pointer to the (non-header) data
 * 	- len - The length of the pointed-to data
 * 	- header - The msghdr struct, including auxillary data
 * 	- context - The user-supplied context pointer
 */
typedef void pollop_callback(
	int fd,
	int opcode,
	void *buf,
	int len,
	struct msghdr header,
	void *context
);

/**
 * \brief Polls the given file descriptor for read events,
 * 	performing the read and calling the callback with
 * 	the results.
 *
 * The events field of the pollfd is overwritten with POLLIN. Setting
 * the events field has no effect.
 *
 * If there is no data to read, because of an error, a timeout or a
 * hang-up, the callback isn't called.
 *
 * \param fd The file descriptor to listen for events on.
 * \param callback The callback to call for read events.
 * \param context A context pointer to pass to the callback.
 * \param timeout The maximum amount of time to block on
 * 	the file descriptor.
 *
 * \returns
 * 	If a read, hang-up or file descriptor error occurred,
 * 	the pollfd representing the passed file descriptor is returned.
 *
 *	If a poll error occurred, a pollfd containing a -1 file
 *	descriptor is returned.
 *
 * 	If the timeout was reached with no events, a pollfd containing
 * 	zeroes is returned.
 */
struct pollfd pollopfd(
	struct pollfd fd,
	pollop_callback *callback,
	void *context,
	int timeout
);

/**
 * \brief Polls the given file descriptors for read events,
 * 	performing the read and calling the callback with
 * 	the results.
 *
 * The events field of the pollfd is overwritten with POLLIN. Setting
 * the events field has no effect.
 *
 * If there is no data to read, because of an error, a timeout or a
 * hang-up, the callback isn't called.
 *
 * \param fds A pointer to the first of an array of file descriptors.
 * \param count The number of file descriptors in the array.
 * \param callback The callback to call for read events.
 * \param context A context pointer to pass to the callback.
 * \param timeout The maximum amount of time to block on
 * 	the file descriptor.
 *
 * \returns
 * 	If a read, hang-up or file descriptor error occurred,
 * 	the last processed file descriptor is returned.
 *
 *	If a poll error occurred, a pollfd containing a -1 file
 *	descriptor is returned.
 *
 * 	If the timeout was reached with no events, a pollfd containing
 * 	zeroes is returned.
 */
struct pollfd pollopfds(
	struct pollfd *fds,
	int count,
	pollop_callback *callback,
	void *context,
	int timeout
);

/**
 * \brief Polls the server for read events,
 * 	performing the read and calling the callback with
 * 	the results.
 *
 * If there is no data to read, because of an error or a
 * hang-up, the callback isn't called.
 *
 * \param callback The callback to call for read events.
 * \param context A context pointer to pass to the callback.
 * \param timeout The maximum amount of time to block on
 * 	the file descriptor.
 *
 * \returns
 * 	If a read, hang-up or file descriptor error occurred,
 * 	the pollfd representing the server file descriptor is returned.
 *
 *	If a poll error occurred, a pollfd containing a -1 file
 *	descriptor is returned.
 *
 * 	If the timeout was reached with no events, a pollfd containing
 * 	zeroes is returned.
 */
struct pollfd pollopsrv(
	pollop_callback *callback,
	void *context,
	int timeout
);

/**
 * \brief Polls the server and clients for read events,
 * 	performing the read and calling the callback with
 * 	the results.
 *
 * For each file descriptor, if there is data to read,
 * it is read, processed and the callback is called, then
 * the next file descriptor is processed. If a file descriptor
 * has no data to read, and returns POLLHUP, POLLERR or
 * POLLNVAL, processing is stopped and that pollfd struct is
 * returned. In future calls to pollop(), that file descriptor
 * will not be processed again.
 *
 * \param callback A callback called on each polled file descriptor.
 * \param context A user-supplied context pointer to pass to
 * 	the callback.
 * \param timeout The length of time to poll for, as passed to
 * 	poll(2).
 *
 * \returns
 * 	If a read, hang-up or file descriptor error occurred,
 * 	the last processed file descriptor is returned.
 *
 *	If a poll error occurred, a pollfd containing a -1 file
 *	descriptor is returned.
 *
 * 	If the timeout was reached with no events, a pollfd containing
 * 	zeroes is returned.
 */
struct pollfd pollop(
	pollop_callback *callback,
	void *context,
	int timeout
);

/**
 * \brief Convenience function for closing all file descriptors
 * 	passed in ancillary data.
 */
void close_cmsg_fds(struct msghdr header);

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
 * \param fds The buffer to write the pollfds into.
 * \param buflen The number of pollfd structs the buffer
 * 	can hold.
 *
 * \returns The number of pollfds initialized, or -1
 * 	if an error was encountered.
 *
 * \sa cli_polls
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
 *
 * \sa srvcli_polls
 */
int cli_polls(struct pollfd *buffer, int buflen);

struct clistate {
	int socket;
	int pid;
};

/**
 * \brief Forks and executes the given command as a new client.
 *
 * Example usage:
 *
 * \code
 * struct clistate client = cliexec(
 * 	"/usr/bin/ls",
 * 	"/usr/bin/ls",
 * 	"-a",
 * 	"/etc",
 * 	NULL
 * );
 * // use client.socket to read/write to the client, and
 * // client.pid to get the client's process ID
 * \endcode
 *
 * \param command The path to an executable.
 * \param arg0 Arguments to pass to the command. Traditionally,
 * 	the arg0 argument is the same as path. Must be terminated
 * 	with a null terminator.
 *
 * \returns A new client socket and process ID for the
 * 	client.
 */
struct clistate cliexecl(const char *path, const char *arg0, ...);

/**
 * \brief Forks and executes the given command as a new client.
 *
 * Example usage:
 *
 * \code
 * char *const envp[] = {
 * 	"PATH=/usr/bin",
 * 	NULL,
 * };
 * struct clistate client = cliexec(
 * 	"/usr/bin/ls",
 * 	"/usr/bin/ls",
 * 	"-a",
 * 	"/etc",
 * 	NULL,
 * 	envp
 * );
 * // use client.socket to read/write to the client, and
 * // client.pid to get the client's process ID
 * \endcode
 *
 * \param command The path to an executable.
 * \param arg0 Arguments to pass to the command. Traditionally,
 * 	the arg0 argument is the same as path. Must be terminated
 * 	with a null terminator.
 *
 * \returns A new client socket and process ID for the
 * 	client.
 */
struct clistate cliexecle(const char *path, const char *arg0, ...);

/**
 * \brief Forks and executes the given command as a new client.
 *
 * Example usage:
 *
 * \code
 * const char *argv[] = {
 * 	"/usr/bin/ls",
 * 	"-a",
 * 	"/etc",
 * 	NULL,
 * };
 * struct clistate client = cliexec(argv[0], argv);
 * // use client.socket to read/write to the client, and
 * // client.pid to get the client's process ID
 * \endcode
 *
 * \param command The path to an executable.
 * \param argv Arguments to pass to the command. Traditionally,
 * 	the first argument is the same as path.
 *
 * \returns A new client socket and process ID for the
 * 	client.
 */
struct clistate cliexecv(const char *path, char *const argv[]);

struct clistate cliexecve(const char *path, char *const argv[], char *const envp[]);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SRVSH_SRVSH
