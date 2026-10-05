## Process this file with automake to produce Makefile.in.

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = rpshow
rpshow_SOURCES = src/rpshow.c

dist_man_MANS = doc/rpshow.1

TESTS = \
	tests/help-version.sh \
	tests/usage.sh \
	tests/show.sh \
	tests/fail.sh
TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)
AM_TESTS_ENVIRONMENT = \
	top_srcdir='$(abs_top_srcdir)'; top_builddir='$(abs_top_builddir)'; \
	export top_srcdir top_builddir;

dist_check_SCRIPTS = $(TESTS) tests/fake-ssh tests/bin/alacritty
dist_check_DATA = tests/init.sh

clean-local:
	-rm -rf tests/t-*
