/*
 * srvsh - A server/client shell script interpreter
 * Copyright (C) 2025  Marcus Harrison
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

#ifndef GUISH_PARSE
#define GUISH_PARSE

#include <libadt/lptr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 */

/**
 * \brief Parses a list of statements, starting a single
 * 	srvsh process to manage them and returning the
 * 	PID of the spawned shell.
 */
int srvsh_parse_script(struct libadt_const_lptr script);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GUISH_PARSE
