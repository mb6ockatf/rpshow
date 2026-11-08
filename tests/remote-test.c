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
#include "zathura.h"

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

static char const *const markdown_names[] = {
  "README.md",
  "notes.MD",
  "a.b.markdown",
  "x.Markdown",
  "y.mdown",
  "z.mkd",
  "z.mkdn",
  "wiki.mdwn",
  "with space.md",
  "it's.md",
  "dir/sub.md",
};

static char const *const other_names[] = {
  "",
  "md",
  ".md",
  "dir/.md",
  "README",
  "notes.txt",
  "notes.md.txt",
  "notes.mdx",
  "notes.",
  "notesmd",
  "md.dir/file",
};

static char const *const pdf_names[] = {
  "paper.pdf",
  "PAPER.PDF",
  "a.b.Pdf",
  "with space.pdf",
  "dir/x.pdf",
};

static char const *const other_pdf_names[] = {
  "pdf",
  ".pdf",
  "dir/.pdf",
  "paper.pdf.txt",
  "paper.pdfx",
  "paperpdf",
};

struct setting_case
{
  char const *line;
  char const *expected;
};

static struct setting_case const setting_cases[] = {
  {"set recolor true\n", "set recolor \"true\"\n"},
  {"set recolor-lightcolor       rgba(0,0,0,1)         # bg\n",
   "set recolor-lightcolor \"rgba(0,0,0,1)\"\n"},
  {"  set\tdefault-bg \"#000000\"", "set default-bg \"#000000\"\n"},
  {"set default-fg '#c0caf5' trailing words\n",
   "set default-fg \"#c0caf5\"\n"},
  {"set render-loading-bg \"rgba(1, 2, 3, 0.5)\"\r\n",
   "set render-loading-bg \"rgba(1, 2, 3, 0.5)\"\n"},
  {"set default-bg #000000\n", NULL},
  {"# set recolor true\n", NULL},
  {"set recolor\n", NULL},
  {"set recolor \"\"\n", NULL},
  {"set adjust-open width\n", "set adjust-open \"width\"\n"},
  {"set first-page-column 1:2\n", NULL},
  {"set selection-clipboard clipboard\n", NULL},
  {"map <Button3> exec rm\n", NULL},
  {"include other\n", NULL},
  {"set default-bg \"$(touch pwned)\"\n", NULL},
  {"set default-bg \"red\\\"\nmap x quit\"\n", NULL},
  {"set default-bg \"unterminated\n", NULL},
  {"set default-bg `id`\n", NULL},
  {"set default-bg a;b\n", NULL},
  {"", NULL},
  {"\n", NULL},
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
	       int verbose, char const *less_options, enum view view,
	       char const *settings)
{
  char *command = remote_command (name, display, wait, verbose,
				  less_options, view, settings);
  if (command == NULL)
    {
      fputs ("remote-test: remote_command returned NULL\n", stderr);
      exit (99);
    }
  char const *expected_words[] = {
    "sh", "-c", remote_script, "rpshow", name, display,
    wait ? "1" : "0", verbose ? "1" : "0", less_options,
    view_word (view), settings
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

static void
check_settings (void)
{
  size_t case_count = sizeof setting_cases / sizeof *setting_cases;
  for (size_t case_index = 0; case_index < case_count; case_index++)
    {
      struct setting_case const *test = &setting_cases[case_index];
      char *setting = zathura_setting (test->line);
      if (test->expected == NULL && setting != NULL)
	fail ("zathurarc line passed on", test->line);
      else if (test->expected != NULL
	       && (setting == NULL || strcmp (setting, test->expected) != 0))
	fail ("zathurarc line not passed on as expected", test->line);
      free (setting);
    }

  static char const config_text[] =
    "set recolor true\n"
    "set window-title-basename \"true\"\n"
    "set recolor-darkcolor rgba(192,202,245,1) # fg\n"
    "map <Button1> quit\n";
  static char const expected[] =
    "set recolor \"true\"\n"
    "set recolor-darkcolor \"rgba(192,202,245,1)\"\n";
  static char const light_expected[] =
    "set recolor \"true\"\n"
    "set recolor-darkcolor \"rgba(192,202,245,1)\"\n"
    "set recolor \"false\"\n";
  for (int light = 0; light <= 1; light++)
    {
      FILE *config = fmemopen ((void *) config_text, sizeof config_text - 1,
			       "r");
      if (config == NULL)
	{
	  perror ("remote-test: fmemopen");
	  exit (99);
	}
      char *settings = zathura_settings (config, light);
      fclose (config);
      if (settings == NULL
	  || strcmp (settings, light ? light_expected : expected) != 0)
	fail ("zathura_settings gave the wrong settings", config_text);
      free (settings);
    }
  char *settings = zathura_settings (NULL, 0);
  if (settings == NULL || settings[0] != '\0')
    fail ("zathura_settings without a config is not empty", "");
  free (settings);
  settings = zathura_settings (NULL, 1);
  if (settings == NULL || strcmp (settings, "set recolor \"false\"\n") != 0)
    fail ("zathura_settings without a config ignores --light", "");
  free (settings);
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

  check_command ("plain.txt", ":0", 0, 0, "-R", VIEW_TEXT, "");
  check_command ("plain.txt", ":1", 1, 1, "", VIEW_TEXT, "");
  check_command ("notes.md", ":0", 0, 1, "-R", VIEW_MARKDOWN, "");
  check_command ("paper.pdf", ":0", 1, 0, "-R", VIEW_PDF,
		 "set recolor \"true\"\nset default-bg \"#000000\"\n");
  for (size_t text_index = 0; text_index < text_count; text_index++)
    {
      char const *text = awkward_texts[text_index];
      check_command (text, text, text_index % 2, text_index % 3 == 0, text,
		     (enum view) (text_index % 3), text);
    }
  if (strcmp (view_word (VIEW_TEXT), "text") != 0
      || strcmp (view_word (VIEW_MARKDOWN), "markdown") != 0
      || strcmp (view_word (VIEW_PDF), "pdf") != 0)
    fail ("view_word gives the wrong word", "");

  size_t markdown_count = sizeof markdown_names / sizeof *markdown_names;
  for (size_t name_index = 0; name_index < markdown_count; name_index++)
    if (!markdown_name (markdown_names[name_index]))
      fail ("not taken for Markdown", markdown_names[name_index]);
  size_t other_count = sizeof other_names / sizeof *other_names;
  for (size_t name_index = 0; name_index < other_count; name_index++)
    if (markdown_name (other_names[name_index]))
      fail ("taken for Markdown", other_names[name_index]);

  size_t pdf_count = sizeof pdf_names / sizeof *pdf_names;
  for (size_t name_index = 0; name_index < pdf_count; name_index++)
    if (!pdf_name (pdf_names[name_index]))
      fail ("not taken for PDF", pdf_names[name_index]);
  size_t other_pdf_count = sizeof other_pdf_names / sizeof *other_pdf_names;
  for (size_t name_index = 0; name_index < other_pdf_count; name_index++)
    if (pdf_name (other_pdf_names[name_index]))
      fail ("taken for PDF", other_pdf_names[name_index]);
  if (!pdf_header ("%PDF-1.4\n", 9) || !pdf_header ("%PDF-", 5))
    fail ("PDF header not recognised", "%PDF-");
  if (pdf_header ("%PDF", 4) || pdf_header ("%PDF-1.4", 0)
      || pdf_header ("%!PS-Adobe", 10) || pdf_header (" %PDF-1.4", 9))
    fail ("taken for a PDF header", "%PDF");

  check_settings ();

  if (failures != 0)
    {
      fprintf (stderr, "remote-test: %d failure(s)\n", failures);
      return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}
