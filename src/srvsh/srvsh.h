/*
 * GuiSH - A Shell for Wayland Programs
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
 */
int cli_end(void);

/**
 * \brief A header prefix for IPC between servers/clients.
 */
struct srvsh_header {
	int opcode;
	int size;
};

/**
 * \brief Returns a pointer to the opcode database
 * 	defined by the environment variable SRVSH_DATABASE.
 *
 * \returns NULL if SRVSH_DATABASE is not set, or the path cannot
 * 	be opened for reading. Returns a pointer to the database
 * 	otherwise.
 */
opcode_db *open_opcode_db(void);

/**
 * \brief Release an opcode database opened with
 * 	open_opcode_db().
 *
 * \param db A database previously opened with open_opcode_db(),
 * 	or NULL to close the statically-allocated database used by
 * 	writesrv() etc.
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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SRVSH_SRVSH
