/* rpshow - show a local file in a fullscreen pager on a Raspberry Pi screen

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
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "remote.h"
#include "zathura.h"
#define DEFAULT_HOST "pi"
#define DEFAULT_DISPLAY ":0"
#define DEFAULT_LESS_OPTIONS "-R"

static char const *program_name = PACKAGE;

static struct option const long_options[] = {
  {"host", required_argument, NULL, 'H'},
  {"display", required_argument, NULL, 'd'},
  {"wait", no_argument, NULL, 'w'},
  {"raw", no_argument, NULL, 'r'},
  {"light", no_argument, NULL, 'l'},
  {"verbose", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'V'},
  {NULL, 0, NULL, 0}
};

static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    {
      fprintf (stderr, "Try '%s --help' for more information.\n",
	       program_name);
      exit (status);
    }
  printf ("Usage: %s [OPTION]... FILE\n", program_name);
  fputs ("\
Show the local FILE in less, in a fullscreen terminal window on the\n\
screen of a remote host (a Raspberry Pi).  A right click closes it.\n\
A Markdown FILE is rendered with Python's rich module first; a PDF FILE\n\
is shown in zathura, recoloured dark the way your zathurarc says.\n\
\n\
  -H, --host=HOST        ssh destination (default: $RPSHOW_HOST, or "
	 DEFAULT_HOST ")\n\
  -d, --display=DISPLAY  X display on the remote host (default: "
	 DEFAULT_DISPLAY ")\n\
  -w, --wait             wait until the window is closed\n\
  -r, --raw              show a Markdown or PDF FILE as plain text\n\
  -l, --light            show a PDF FILE in its own colours\n\
  -v, --verbose          report dropped less options and zathura warnings\n\
  -h, --help             display this help and exit\n\
  -V, --version          display version information and exit\n\
\n\
Options must come before FILE when POSIXLY_CORRECT is set.\n\
\n\
less gets the options in $RPSHOW_LESS, or else $LESS, or else \""
	 DEFAULT_LESS_OPTIONS "\".\n\
The ssh program is $RPSHOW_SSH, or else ssh.\n\
\n\
Report bugs to: " PACKAGE_BUGREPORT "\n", stdout);
  exit (status);
}

static void
version (void)
{
  printf ("%s (%s) %s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION);
  fputs ("\
Copyright (C) 2026 mb6ockatf\n\
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n", stdout);
  exit (EXIT_SUCCESS);
}

static void
fail_with_errno (char const *what)
{
  fprintf (stderr, "%s: %s: %s\n", program_name, what, strerror (errno));
  exit (EXIT_FAILURE);
}

static enum view
file_view (int file_fd, char const *name, int raw, int *pdf)
{
  char header[PDF_MAGIC_LENGTH];
  ssize_t header_size = pread (file_fd, header, sizeof header, 0);
  *pdf = pdf_name (name)
    || (header_size > 0 && pdf_header (header, (size_t) header_size));
  if (raw)
    return VIEW_TEXT;
  if (*pdf)
    return VIEW_PDF;
  return markdown_name (name) ? VIEW_MARKDOWN : VIEW_TEXT;
}

static char *
remote_name (char const *name, int pdf)
{
  static char const pdf_suffix[] = ".pdf";
  size_t name_length = strlen (name);
  int add_suffix = pdf && !pdf_name (name);
  char *copy = malloc (name_length + sizeof pdf_suffix);
  if (copy == NULL)
    fail_with_errno ("malloc");
  memcpy (copy, name, name_length);
  strcpy (copy + name_length, add_suffix ? pdf_suffix : "");
  return copy;
}

static char *
read_zathura_settings (enum view view, int light)
{
  if (view != VIEW_PDF)
    return NULL;
  char *path = zathura_config_path ();
  if (path == NULL && errno != ENOENT)
    fail_with_errno ("malloc");
  FILE *config = path != NULL ? fopen (path, "r") : NULL;
  if (config == NULL && path != NULL && errno != ENOENT)
    fail_with_errno (path);
  char *settings = zathura_settings (config, light);
  if (settings == NULL)
    fail_with_errno (path != NULL ? path : "malloc");
  if (config != NULL)
    fclose (config);
  free (path);
  return settings;
}

static char const *
getenv_nonempty (char const *name)
{
  char const *value = getenv (name);
  return value != NULL && *value != '\0' ? value : NULL;
}

int
main (int argc, char **argv)
{
  char const *host = getenv_nonempty ("RPSHOW_HOST");
  char const *display = DEFAULT_DISPLAY;
  int wait = 0, raw = 0, light = 0, verbose = 0, option;
  if (argc > 0 && argv[0] != NULL && *argv[0] != '\0')
    {
      char const *slash = strrchr (argv[0], '/');
      program_name = slash != NULL ? slash + 1 : argv[0];
    }
  if (host == NULL)
    host = DEFAULT_HOST;
  while ((option = getopt_long (argc, argv, "H:d:wrlvhV", long_options, NULL))
	 != -1)
    switch (option)
      {
      case 'H':
	host = optarg;
	break;
      case 'd':
	display = optarg;
	break;
      case 'w':
	wait = 1;
	break;
      case 'r':
	raw = 1;
	break;
      case 'l':
	light = 1;
	break;
      case 'v':
	verbose = 1;
	break;
      case 'h':
	usage (EXIT_SUCCESS);
	break;
      case 'V':
	version ();
	break;
      default:
	usage (EXIT_FAILURE);
      }

  if (optind == argc)
    {
      fprintf (stderr, "%s: missing file operand\n", program_name);
      usage (EXIT_FAILURE);
    }
  if (optind + 1 < argc)
    {
      fprintf (stderr, "%s: extra operand '%s'\n", program_name,
	       argv[optind + 1]);
      usage (EXIT_FAILURE);
    }
  if (*host == '\0')
    {
      fprintf (stderr, "%s: empty host name\n", program_name);
      return EXIT_FAILURE;
    }
  char const *file = argv[optind];
  int file_fd = open (file, O_RDONLY | O_NOCTTY);
  struct stat file_status;
  if (file_fd < 0 || fstat (file_fd, &file_status) != 0)
    {
      fprintf (stderr, "%s: %s: %s\n", program_name, file, strerror (errno));
      return EXIT_FAILURE;
    }
  if (!S_ISREG (file_status.st_mode))
    {
      fprintf (stderr, "%s: %s: not a regular file\n", program_name, file);
      return EXIT_FAILURE;
    }
  char const *name = strrchr (file, '/');
  name = name != NULL ? name + 1 : file;
  char const *less_options = getenv_nonempty ("RPSHOW_LESS");
  if (less_options == NULL)
    less_options = getenv_nonempty ("LESS");
  if (less_options == NULL)
    less_options = DEFAULT_LESS_OPTIONS;
  char const *ssh = getenv_nonempty ("RPSHOW_SSH");
  if (ssh == NULL)
    ssh = "ssh";
  int pdf;
  enum view view = file_view (file_fd, name, raw, &pdf);
  char *shown_name = remote_name (name, pdf);
  char *settings = read_zathura_settings (view, light);
  char *command = remote_command (shown_name, display, wait, verbose,
				  less_options, view,
				  settings != NULL ? settings : "");
  free (settings);
  free (shown_name);
  if (command == NULL)
    {
      fprintf (stderr, "%s: memory exhausted\n", program_name);
      return EXIT_FAILURE;
    }
  char *ssh_argv[] = { (char *) ssh, (char *) "-T", (char *) "--",
		       (char *) host, command, NULL };
  fflush (stdout);
  pid_t ssh_pid = fork ();
  if (ssh_pid < 0)
    {
      fprintf (stderr, "%s: fork: %s\n", program_name, strerror (errno));
      return EXIT_FAILURE;
    }
  if (ssh_pid == 0)
    {
      if (file_fd != STDIN_FILENO)
	{
	  if (dup2 (file_fd, STDIN_FILENO) < 0)
	    {
	      fprintf (stderr, "%s: dup2: %s\n", program_name,
		       strerror (errno));
	      _exit (EXIT_FAILURE);
	    }
	  close (file_fd);
	}
      execvp (ssh, ssh_argv);
      int exec_errno = errno;
      fprintf (stderr, "%s: %s: %s\n", program_name, ssh,
	       strerror (exec_errno));
      _exit (exec_errno == ENOENT ? 127 : 126);
    }
  close (file_fd);
  free (command);
  int status;
  while (waitpid (ssh_pid, &status, 0) < 0)
    if (errno != EINTR)
      {
	fprintf (stderr, "%s: waitpid: %s\n", program_name, strerror (errno));
	return EXIT_FAILURE;
      }
  if (WIFEXITED (status))
    return WEXITSTATUS (status);
  fprintf (stderr, "%s: %s killed by signal %d\n", program_name, ssh,
	   WTERMSIG (status));
  return 128 + WTERMSIG (status);
}
