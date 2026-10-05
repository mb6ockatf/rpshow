/* remote.h - build the command rpshow runs on the remote host

   Copyright (C) 2026 mb6ockatf

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef RPSHOW_REMOTE_H
#define RPSHOW_REMOTE_H
#include <stddef.h>
extern char const remote_script[];
size_t quoted_length (char const *text);
char *append_quoted (char *cursor, char const *text);
char *remote_command (char const *name, char const *display, int wait,
		      int verbose, char const *less_options);
#endif
