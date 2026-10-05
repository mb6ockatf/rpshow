/* remote-test - unit tests for the quoting in src/remote.c

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "remote.h"

static int failures;

static char const *const awkward_texts[] = {
  "",
  "plain",
  "with space",
  "  leading and trailing  ",
  "it's",
  "'",
  "''",
  "'''",
  "a'b'c",
  "trailing '",
  "'\\''",
  "\"double\"",
  "$HOME",
  "${HOME}",
  "$(echo injected)",
  "`echo injected`",
  "back\\slash",
  "\\'",
  "new\nline",
  "\n",
  "tab\there",
  "*",
  "?",
  "[a-z]",
  "~",
  "-n",
  "--",
  "!event",
  "a;b|c&d>e<f",
  "%s%d%n",
  "\xc3\xbc\xc3\xb1\xc3\xad",
};

static void *
allocate (size_t size)
{
  void *block = malloc (size);
  if (block == NULL)
    {
      fputs ("remote-test: memory exhausted\n", stderr);
      exit (99);
    }
  return block;
}

static void
fail (char const *what, char const *text)
{
  fprintf (stderr, "FAIL: %s: [%s]\n", what, text);
  failures++;
}

static char *
shell_output (char const *command, size_t *output_length)
{
  FILE *stream = popen (command, "r");
  if (stream == NULL)
    {
      perror ("remote-test: popen");
      exit (99);
    }
  size_t capacity = 4096;
  size_t used = 0;
  char *output = allocate (capacity);
  size_t count;
  while ((count = fread (output + used, 1, capacity - used, stream)) > 0)
    {
      used += count;
      if (used == capacity)
	{
	  capacity *= 2;
	  char *bigger = realloc (output, capacity);
	  if (bigger == NULL)
	    {
	      fputs ("remote-test: memory exhausted\n", stderr);
	      exit (99);
	    }
	  output = bigger;
	}
    }
  if (pclose (stream) != 0)
    {
      fprintf (stderr, "remote-test: shell failed: %s\n", command);
      exit (99);
    }
  *output_length = used;
  return output;
}

static char *
quote (char const *text)
{
  size_t length = quoted_length (text);
  char *quoted = allocate (length + 1);
  char *end = append_quoted (quoted, text);
  if ((size_t) (end - quoted) != length)
    fail ("quoted_length differs from what append_quoted wrote", text);
  *end = '\0';
  return quoted;
}

static void
check_quoting (char const *text)
{
  char *quoted = quote (text);
  if (quoted[0] != '\'' || quoted[strlen (quoted) - 1] != '\'')
    fail ("not wrapped in single quotes", text);
  static char const printf_prefix[] = "printf %s ";
  char *command = allocate (sizeof printf_prefix + strlen (quoted));
  strcpy (command, printf_prefix);
  strcat (command, quoted);
  size_t output_length;
  char *output = shell_output (command, &output_length);
  if (output_length != strlen (text)
      || memcmp (output, text, output_length) != 0)
    fail ("the shell did not get the text back", text);
  free (output);
  free (command);
  free (quoted);
}

static void
check_command (char const *name, char const *display, int wait,
	       int verbose, char const *less_options)
{
  char *command = remote_command (name, display, wait, verbose,
				  less_options);
  if (command == NULL)
    {
      fputs ("remote-test: remote_command returned NULL\n", stderr);
      exit (99);
    }
  char const *expected_words[] = {
    "sh", "-c", remote_script, "rpshow", name, display,
    wait ? "1" : "0", verbose ? "1" : "0", less_options
  };
  size_t word_count = sizeof expected_words / sizeof *expected_words;
  static char const printf_prefix[] = "printf '%s\\0' ";
  char *printer = allocate (sizeof printf_prefix + strlen (command));
  strcpy (printer, printf_prefix);
  strcat (printer, command);
  size_t output_length;
  char *output = shell_output (printer, &output_length);
  char const *word = output;
  char const *output_end = output + output_length;
  for (size_t word_index = 0; word_index < word_count; word_index++)
    {
      if (word >= output_end)
	{
	  fail ("remote command has too few words", name);
	  break;
	}
      if (strcmp (word, expected_words[word_index]) != 0)
	fail ("remote command word differs", expected_words[word_index]);
      word += strlen (word) + 1;
    }
  if (word < output_end)
    fail ("remote command has extra words", name);
  free (output);
  free (printer);
  free (command);
}

int
main (void)
{
  size_t text_count = sizeof awkward_texts / sizeof *awkward_texts;
  for (size_t text_index = 0; text_index < text_count; text_index++)
    check_quoting (awkward_texts[text_index]);
  check_quoting (remote_script);

  if (quoted_length ("") != 2)
    fail ("quoted_length of the empty string is not 2", "");
  if (quoted_length ("'") != 6)
    fail ("quoted_length of a lone quote is not 6", "'");

  check_command ("plain.txt", ":0", 0, 0, "-R");
  check_command ("plain.txt", ":1", 1, 1, "");
  for (size_t text_index = 0; text_index < text_count; text_index++)
    {
      char const *text = awkward_texts[text_index];
      check_command (text, text, text_index % 2, text_index % 3 == 0, text);
    }

  if (failures != 0)
    {
      fprintf (stderr, "remote-test: %d failure(s)\n", failures);
      return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}
