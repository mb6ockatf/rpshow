/* zathura.c - pick the view settings rpshow passes on to zathura

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
#include "zathura.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define SETTING_WORDS 3
#define SETTING_OVERHEAD 16

static char const *const passed_keys[] = {
  "adjust-open",
  "recolor",
  "recolor-keephue",
  "recolor-reverse-video",
  "recolor-lightcolor",
  "recolor-darkcolor",
  "default-bg",
  "default-fg",
  "render-loading",
  "render-loading-bg",
  "render-loading-fg",
};

static char const value_characters[] =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
  "#(),.%+- ";

static char const light_setting[] = "set recolor \"false\"\n";

char *
zathura_config_path (void)
{
  static char const xdg_suffix[] = "/zathura/zathurarc";
  static char const home_suffix[] = "/.config/zathura/zathurarc";
  char const *base = getenv ("XDG_CONFIG_HOME");
  char const *suffix = xdg_suffix;
  if (base == NULL || base[0] != '/')
    {
      base = getenv ("HOME");
      suffix = home_suffix;
    }
  if (base == NULL || base[0] == '\0')
    {
      errno = ENOENT;
      return NULL;
    }
  size_t base_length = strlen (base);
  char *path = malloc (base_length + strlen (suffix) + 1);
  if (path == NULL)
    return NULL;
  memcpy (path, base, base_length);
  strcpy (path + base_length, suffix);
  return path;
}

static int
line_end (char character)
{
  return character == '\0' || character == '\n' || character == '\r';
}

static int
blank (char character)
{
  return character == ' ' || character == '\t';
}

static size_t
split_words (char *line, char **words)
{
  size_t word_count = 0;
  char *reader = line;
  char *writer = line;
  while (word_count < SETTING_WORDS)
    {
      while (blank (*reader))
	reader++;
      if (line_end (*reader) || *reader == '#')
	break;
      words[word_count++] = writer;
      char quote = '\0';
      for (; !line_end (*reader); reader++)
	{
	  if (quote == '\0' && blank (*reader))
	    break;
	  if (quote == '\0' && (*reader == '"' || *reader == '\''))
	    quote = *reader;
	  else if (quote != '\0' && *reader == quote)
	    quote = '\0';
	  else
	    {
	      if (*reader == '\\' && quote != '\'' && !line_end (reader[1]))
		reader++;
	      *writer++ = *reader;
	    }
	}
      if (quote != '\0')
	return 0;
      int finished = line_end (*reader);
      *writer++ = '\0';
      if (finished)
	break;
      reader++;
    }
  return word_count;
}

static int
passed_key (char const *key)
{
  for (size_t key_index = 0;
       key_index < sizeof passed_keys / sizeof *passed_keys; key_index++)
    if (strcmp (key, passed_keys[key_index]) == 0)
      return 1;
  return 0;
}

char *
zathura_setting (char const *line)
{
  size_t line_length = strlen (line);
  char *scratch = malloc (line_length + 1);
  if (scratch == NULL)
    return NULL;
  memcpy (scratch, line, line_length + 1);
  char *words[SETTING_WORDS];
  char *setting = NULL;
  errno = 0;
  if (split_words (scratch, words) == SETTING_WORDS
      && strcmp (words[0], "set") == 0 && passed_key (words[1])
      && words[2][0] != '\0'
      && strspn (words[2], value_characters) == strlen (words[2]))
    {
      size_t size = line_length + SETTING_OVERHEAD;
      setting = malloc (size);
      if (setting != NULL)
	snprintf (setting, size, "set %s \"%s\"\n", words[1], words[2]);
    }
  free (scratch);
  return setting;
}

static int
append (char **text, size_t *length, char const *addition)
{
  size_t addition_length = strlen (addition);
  char *bigger = realloc (*text, *length + addition_length + 1);
  if (bigger == NULL)
    return 0;
  memcpy (bigger + *length, addition, addition_length + 1);
  *text = bigger;
  *length += addition_length;
  return 1;
}

char *
zathura_settings (FILE *config, int light)
{
  char *settings = calloc (1, 1);
  size_t settings_length = 0;
  char *line = NULL;
  size_t line_capacity = 0;
  if (settings == NULL)
    return NULL;
  while (config != NULL
	 && (errno = 0, getline (&line, &line_capacity, config) >= 0))
    {
      char *setting = zathura_setting (line);
      if (setting == NULL && errno != 0)
	goto failure;
      if (setting != NULL)
	{
	  int appended = append (&settings, &settings_length, setting);
	  free (setting);
	  if (!appended)
	    goto failure;
	}
    }
  if (config != NULL && (ferror (config) || errno == ENOMEM))
    goto failure;
  if (light && !append (&settings, &settings_length, light_setting))
    goto failure;
  free (line);
  return settings;

failure:
  {
    int saved_errno = errno;
    free (line);
    free (settings);
    errno = saved_errno;
    return NULL;
  }
}
