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

#ifndef GUISH_PROCESS
#define GUISH_PROCESS

#include <libadt/lptr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 */

int fork_wrapper(struct libadt_const_lptr statement, int cli);
int exec_command(struct libadt_const_lptr statement);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GUISH_PROCESS
