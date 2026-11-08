/* zathura.h - pick the view settings rpshow passes on to zathura

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

#ifndef RPSHOW_ZATHURA_H
#define RPSHOW_ZATHURA_H
#include <stdio.h>
char *zathura_config_path (void);
char *zathura_setting (char const *line);
char *zathura_settings (FILE *config, int light);
#endif
