/* remote.c - build the command rpshow runs on the remote host

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

#include <config.h>
#include "remote.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

char const remote_script[] =
  "set -f\n"
  "name=$1 display=$2 wait=$3 verbose=$4 opts=$5 view=$6 zathura=$7\n"
  "unset LC_ALL LESS\n"
  "if [ \"$view\" = pdf ] && ! command -v zathura >/dev/null 2>&1; then\n"
  "  echo \"rpshow: $(uname -n): zathura not found;"
  " showing the PDF as text\" >&2\n"
  "  view=text\n"
  "fi\n"
  "if [ \"$view\" != pdf ] && ! command -v alacritty >/dev/null 2>&1; then\n"
  "  echo \"rpshow: $(uname -n): alacritty: command not found\" >&2\n"
  "  exit 127\n"
  "fi\n"
  "d=$(mktemp -d \"${TMPDIR:-/tmp}/rpshow.XXXXXX\") || exit 1\n"
  "case $d in /*) ;; *) d=$PWD/$d ;; esac\n"
  "f=$d/$name\n"
  "if ! cat >\"$f\"; then rm -rf \"$d\"; exit 1; fi\n"
  "export LESSCHARSET=utf-8 LESSSECURE=1 LESSSECURE_ALLOW=lessopen\n"
  "if command -v lesspipe >/dev/null 2>&1; then\n"
  "  eval \"$(SHELL=/bin/sh lesspipe)\"\n"
  "fi\n"
  "keep=\n"
  "for o in $opts; do\n"
  "  case $o in\n"
  "  --mouse | --MOUSE) err='mouse reporting hides the right click' ;;\n"
  "  *) err=$(less \"$o\" </dev/null 2>&1 >/dev/null) ;;\n"
  "  esac\n"
  "  if [ -z \"$err\" ]; then\n"
  "    keep=\"$keep $o\"\n"
  "  elif [ \"$verbose\" = 1 ]; then\n"
  "    echo \"rpshow: dropping less option $o: $err\" >&2\n"
  "  fi\n"
  "done\n"
  "export LESS=\"${keep# }\" DISPLAY=\"$display\"\n"
  "if [ \"$view\" = markdown ] &&"
  " ! python3 -c 'import rich.markdown' >/dev/null 2>&1; then\n"
  "  echo \"rpshow: $(uname -n): python3 with the rich module"
  " not found; showing the Markdown source\" >&2\n"
  "  view=text\n"
  "fi\n"
  "if [ \"$view\" = pdf ]; then\n"
  "  printf '%s\\n' 'set guioptions \"\"' 'set recolor true'"
  " 'map [normal] <Button3> quit' 'map [fullscreen] <Button3> quit'"
  " \"$zathura\" >\"$d/zathurarc\"\n"
  "fi\n"
  "(\n"
  "  trap '' HUP\n"
  "  case $view in\n"
  "  pdf)\n"
  "    set -- zathura --mode=fullscreen --config-dir=\"$d\""
  " --data-dir=\"$d\" --cache-dir=\"$d\" \"$f\" ;;\n"
  "  *)\n"
  "    if [ \"$view\" = markdown ]; then\n"
  "      set -- sh -c 'PYTHONIOENCODING=utf-8"
  " python3 -m rich.markdown -c \"$1\" | less -R' rpshow \"$f\"\n"
  "    else\n"
  "      set -- less \"$f\"\n"
  "    fi\n"
  "    set -- alacritty --class rpshow"
  " -o 'window.startup_mode=\"Fullscreen\"' -o window.opacity=1.0"
  " -o 'mouse.bindings=[{mouse=\"Right\",action=\"Quit\"}]'"
  " -e \"$@\" ;;\n"
  "  esac\n"
  "  setsid \"$@\" >\"$d/log\" 2>&1 </dev/null &\n"
  "  echo $! >\"$d/pid\"\n"
  "  wait $!\n"
  "  s=$?\n"
  "  echo \"$s\" >\"$d/status\"\n"
  "  if [ \"$wait\" != 1 ]; then\n"
  "    [ \"$s\" -eq 0 ] || sleep 5\n"
  "    rm -rf \"$d\"\n"
  "  fi\n"
  ") >/dev/null 2>&1 </dev/null &\n"
  "watcher=$!\n"
  "sleep 1\n"
  "if [ \"$view\" = pdf ] && [ \"$verbose\" = 1 ]; then\n"
  "  sed -n 's/^warning: /rpshow: zathura: /p' \"$d/log\" >&2 2>/dev/null\n"
  "fi\n"
  "if [ \"$view\" != pdf ] &&"
  " grep -q -e 'Ignoring invalid CLI option' -e 'Unable to override option'"
  " -e 'Config error' \"$d/log\" 2>/dev/null; then\n"
  "  echo \"rpshow: alacritty rejected its settings; window closed\" >&2\n"
  "  cat \"$d/log\" >&2\n"
  "  kill \"$(cat \"$d/pid\")\" 2>/dev/null\n"
  "  if [ \"$wait\" = 1 ]; then wait \"$watcher\"; rm -rf \"$d\"; fi\n"
  "  exit 1\n"
  "fi\n"
  "if [ \"$wait\" = 1 ]; then\n"
  "  wait \"$watcher\"\n"
  "  s=$(cat \"$d/status\")\n"
  "  [ \"$s\" -eq 0 ] || cat \"$d/log\" >&2\n"
  "  rm -rf \"$d\"\n"
  "  exit \"$s\"\n"
  "fi\n"
  "if [ -e \"$d/status\" ] && [ \"$(cat \"$d/status\")\" -ne 0 ]; then\n"
  "  cat \"$d/log\" >&2\n"
  "  exit \"$(cat \"$d/status\")\"\n"
  "fi\n"
  "exit 0\n";

size_t
quoted_length (char const *text)
{
  size_t length = 2;
  for (; *text != '\0'; text++)
    length += *text == '\'' ? 4 : 1;
  return length;
}

char *
append_quoted (char *cursor, char const *text)
{
  *cursor++ = '\'';
  for (; *text != '\0'; text++)
    if (*text == '\'')
      {
	memcpy (cursor, "'\\''", 4);
	cursor += 4;
      }
    else
      *cursor++ = *text;
  *cursor++ = '\'';
  return cursor;
}

static int
has_suffix (char const *name, char const *const *suffixes,
	    size_t suffix_count)
{
  char const *dot = strrchr (name, '.');
  if (dot == NULL || dot == name || dot[-1] == '/')
    return 0;
  for (size_t suffix_index = 0; suffix_index < suffix_count; suffix_index++)
    if (strcasecmp (dot + 1, suffixes[suffix_index]) == 0)
      return 1;
  return 0;
}

int
markdown_name (char const *name)
{
  static char const *const suffixes[] = {
    "md", "markdown", "mdown", "mkd", "mkdn", "mdwn"
  };
  return has_suffix (name, suffixes, sizeof suffixes / sizeof *suffixes);
}

int
pdf_name (char const *name)
{
  static char const *const suffixes[] = { "pdf" };
  return has_suffix (name, suffixes, sizeof suffixes / sizeof *suffixes);
}

int
pdf_header (char const *bytes, size_t size)
{
  return size >= PDF_MAGIC_LENGTH
    && memcmp (bytes, PDF_MAGIC, PDF_MAGIC_LENGTH) == 0;
}

char const *
view_word (enum view view)
{
  switch (view)
    {
    case VIEW_MARKDOWN:
      return "markdown";
    case VIEW_PDF:
      return "pdf";
    case VIEW_TEXT:
    default:
      return "text";
    }
}

char *
remote_command (char const *name, char const *display, int wait,
		int verbose, char const *less_options, enum view view,
		char const *zathura_settings)
{
  static char const prefix[] = "sh -c ";
  static char const argv0[] = " rpshow";
  char const *args[] = { name, display, wait ? "1" : "0",
			 verbose ? "1" : "0", less_options, view_word (view),
			 zathura_settings };
  size_t length = sizeof prefix + quoted_length (remote_script) + sizeof argv0;
  for (size_t arg_index = 0; arg_index < sizeof args / sizeof *args;
       arg_index++)
    length += 1 + quoted_length (args[arg_index]);
  char *command = malloc (length);
  if (command == NULL)
    return NULL;
  char *cursor = command;
  memcpy (cursor, prefix, sizeof prefix - 1);
  cursor = append_quoted (cursor + sizeof prefix - 1, remote_script);
  memcpy (cursor, argv0, sizeof argv0 - 1);
  cursor += sizeof argv0 - 1;
  for (size_t arg_index = 0; arg_index < sizeof args / sizeof *args;
       arg_index++)
    {
      *cursor++ = ' ';
      cursor = append_quoted (cursor, args[arg_index]);
    }
  *cursor = '\0';
  return command;
}
