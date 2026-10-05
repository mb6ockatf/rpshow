#!/bin/sh
# Check --help and --version.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

"$rpshow" --help > "$work/out" || fail '--help failed'
grep '^Usage: rpshow ' "$work/out" > /dev/null || fail '--help: no usage line'
"$rpshow" --version > "$work/out" || fail '--version failed'
head -n 1 "$work/out" | grep '^rpshow (rpshow) ' > /dev/null ||
	fail '--version: bad first line'
test ! -e "$work/ssh.args" || fail 'ssh was run'
exit 0
