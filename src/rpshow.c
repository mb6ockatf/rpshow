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

#define DEFAULT_HOST "pi"
#define DEFAULT_DISPLAY ":0"
#define DEFAULT_LESS_OPTIONS "-R"

static char const remote_script[] =
  "set -f\n"
  "name=$1 display=$2 wait=$3 verbose=$4 opts=$5\n"
  "unset LC_ALL LESS\n"
  "if ! command -v alacritty >/dev/null 2>&1; then\n"
  "  echo \"rpshow: $(uname -n): alacritty: command not found\" >&2\n"
  "  exit 127\n"
  "fi\n"
  "d=$(mktemp -d \"${TMPDIR:-/tmp}/rpshow.XXXXXX\") || exit 1\n"
  "case $d in /*) ;; *) d=$PWD/$d ;; esac\n"
  "f=$d/$name\n"
  "if ! cat >\"$f\"; then rm -rf \"$d\"; exit 1; fi\n"
  "export LESSCHARSET=utf-8 LESSSECURE=1\n"
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
  "(\n"
  "  trap '' HUP\n"
  "  setsid alacritty --class rpshow"
  " -o 'window.startup_mode=\"Fullscreen\"' -o window.opacity=1.0"
  " -o 'mouse.bindings=[{mouse=\"Right\",action=\"Quit\"}]'"
  " -e less \"$f\" >\"$d/log\" 2>&1 </dev/null &\n"
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
  "if grep -q -e 'Ignoring invalid CLI option' -e 'Unable to override option'"
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

static char const *program_name = PACKAGE;

static struct option const long_options[] = {
  {"host", required_argument, NULL, 'H'},
  {"display", required_argument, NULL, 'd'},
  {"wait", no_argument, NULL, 'w'},
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
\n\
  -H, --host=HOST        ssh destination (default: $RPSHOW_HOST, or "
	 DEFAULT_HOST ")\n\
  -d, --display=DISPLAY  X display on the remote host (default: "
	 DEFAULT_DISPLAY ")\n\
  -w, --wait             wait until the window is closed\n\
  -v, --verbose          report less options the remote less lacks\n\
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

static void *
xmalloc (size_t n)
{
  void *p = malloc (n);
  if (p == NULL)
    {
      fprintf (stderr, "%s: memory exhausted\n", program_name);
      exit (EXIT_FAILURE);
    }
  return p;
}

static size_t
quoted_length (char const *s)
{
  size_t n = 2;
  for (; *s != '\0'; s++)
    n += *s == '\'' ? 4 : 1;
  return n;
}

static char *
append_quoted (char *p, char const *s)
{
  *p++ = '\'';
  for (; *s != '\0'; s++)
    if (*s == '\'')
      {
	memcpy (p, "'\\''", 4);
	p += 4;
      }
    else
      *p++ = *s;
  *p++ = '\'';
  return p;
}

static char *
remote_command (char const *name, char const *display, int wait,
		int verbose, char const *less_options)
{
  static char const prefix[] = "sh -c ";
  static char const argv0[] = " rpshow";
  char const *args[] = { name, display, wait ? "1" : "0",
			 verbose ? "1" : "0", less_options };
  size_t n = sizeof prefix + quoted_length (remote_script) + sizeof argv0;
  for (size_t i = 0; i < sizeof args / sizeof *args; i++)
    n += 1 + quoted_length (args[i]);

  char *cmd = xmalloc (n);
  char *p = cmd;
  memcpy (p, prefix, sizeof prefix - 1);
  p = append_quoted (p + sizeof prefix - 1, remote_script);
  memcpy (p, argv0, sizeof argv0 - 1);
  p += sizeof argv0 - 1;
  for (size_t i = 0; i < sizeof args / sizeof *args; i++)
    {
      *p++ = ' ';
      p = append_quoted (p, args[i]);
    }
  *p = '\0';
  return cmd;
}

static char const *
getenv_nonempty (char const *name)
{
  char const *v = getenv (name);
  return v != NULL && *v != '\0' ? v : NULL;
}

int
main (int argc, char **argv)
{
  char const *host = getenv_nonempty ("RPSHOW_HOST");
  char const *display = DEFAULT_DISPLAY;
  int wait = 0;
  int verbose = 0;
  int c;

  if (argc > 0 && argv[0] != NULL && *argv[0] != '\0')
    {
      char const *slash = strrchr (argv[0], '/');
      program_name = slash != NULL ? slash + 1 : argv[0];
    }
  if (host == NULL)
    host = DEFAULT_HOST;

  while ((c = getopt_long (argc, argv, "H:d:wvhV", long_options, NULL))
	 != -1)
    switch (c)
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
  int fd = open (file, O_RDONLY | O_NOCTTY);
  struct stat st;
  if (fd < 0 || fstat (fd, &st) != 0)
    {
      fprintf (stderr, "%s: %s: %s\n", program_name, file, strerror (errno));
      return EXIT_FAILURE;
    }
  if (!S_ISREG (st.st_mode))
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

  char *cmd = remote_command (name, display, wait, verbose, less_options);
  char *ssh_argv[] = { (char *) ssh, (char *) "-T", (char *) "--",
		       (char *) host, cmd, NULL };

  fflush (stdout);
  pid_t pid = fork ();
  if (pid < 0)
    {
      fprintf (stderr, "%s: fork: %s\n", program_name, strerror (errno));
      return EXIT_FAILURE;
    }
  if (pid == 0)
    {
      if (fd != STDIN_FILENO)
	{
	  if (dup2 (fd, STDIN_FILENO) < 0)
	    {
	      fprintf (stderr, "%s: dup2: %s\n", program_name,
		       strerror (errno));
	      _exit (EXIT_FAILURE);
	    }
	  close (fd);
	}
      execvp (ssh, ssh_argv);
      int e = errno;
      fprintf (stderr, "%s: %s: %s\n", program_name, ssh, strerror (e));
      _exit (e == ENOENT ? 127 : 126);
    }

  close (fd);
  free (cmd);

  int status;
  while (waitpid (pid, &status, 0) < 0)
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
