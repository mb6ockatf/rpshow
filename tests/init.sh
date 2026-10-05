# init.sh - common setup for the rpshow tests, sourced by each test.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

: "${top_srcdir:?}" "${top_builddir:?}"

rpshow=$top_builddir/rpshow
mkdir -p "$top_builddir/tests" || exit 99
work=$(mktemp -d "$top_builddir/tests/t-XXXXXX") || exit 99
trap 'rm -rf "$work"' 0

mkdir "$work/tmp"
RPSHOW_TEST_DIR=$work
RPSHOW_SSH=$top_srcdir/tests/fake-ssh
PATH=$top_srcdir/tests/bin:$PATH
TMPDIR=$work/tmp
export RPSHOW_TEST_DIR RPSHOW_SSH PATH TMPDIR
unset RPSHOW_HOST RPSHOW_LESS LESS

command -v setsid >/dev/null 2>&1 || {
	echo 'setsid not found; skipping' >&2
	exit 77
}

fail () {
	echo "FAIL: $*" >&2
	exit 1
}

# leftover_tmp: succeed if the fake remote side left files in $TMPDIR.
leftover_tmp () {
	test -n "$(ls -A "$TMPDIR")"
}
