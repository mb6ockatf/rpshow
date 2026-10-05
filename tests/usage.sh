#!/bin/sh
# Check that bad usage fails before ssh is run.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

echo x > "$work/a"
echo y > "$work/b"

"$rpshow" 2> "$work/err" && fail 'no operand succeeded'
grep 'missing file operand' "$work/err" > /dev/null || fail 'no operand: message'
"$rpshow" "$work/a" "$work/b" 2> "$work/err" && fail 'two operands succeeded'
grep 'extra operand' "$work/err" > /dev/null || fail 'two operands: message'
"$rpshow" "$work/nonexistent" 2> "$work/err" && fail 'missing file succeeded'
grep 'No such file' "$work/err" > /dev/null || fail 'missing file: message'
"$rpshow" "$work" 2> "$work/err" && fail 'directory succeeded'
grep 'not a regular file' "$work/err" > /dev/null || fail 'directory: message'
"$rpshow" --bogus "$work/a" 2> /dev/null && fail 'bad option succeeded'
"$rpshow" -H '' "$work/a" 2> /dev/null && fail 'empty host succeeded'
test ! -e "$work/ssh.args" || fail 'ssh was run'
exit 0
