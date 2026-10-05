#!/bin/sh
# Check that a terminal that fails to start is reported.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

echo x > "$work/a"
FAKE_ALACRITTY_STATUS=3
export FAKE_ALACRITTY_STATUS

"$rpshow" -w "$work/a" 2> "$work/err"
test $? -eq 3 || fail 'wait: exit status is not 3'
grep 'cannot open display' "$work/err" > /dev/null || fail 'wait: no log'
leftover_tmp && fail 'wait: remote copy left behind'

"$rpshow" "$work/a" 2> "$work/err"
test $? -eq 3 || fail 'detached: exit status is not 3'
grep 'cannot open display' "$work/err" > /dev/null || fail 'detached: no log'

unset FAKE_ALACRITTY_STATUS
for warning in \
	"Ignoring invalid CLI option 'x=y': TOML parse error" \
	"[ERROR] [alacritty_log_window_config] Unable to override option 'x=1'" \
	'[ERROR] [alacritty_config_derive] Config error: unknown keyboard action'
do
	for mode in -v -w
	do
		rm -f "$work/alacritty.pid"
		rm -rf "$work/tmp$mode"
		mkdir "$work/tmp$mode"
		TMPDIR=$work/tmp$mode FAKE_ALACRITTY_WARN=$warning \
			"$rpshow" $mode "$work/a" 2> "$work/err"
		test $? -eq 1 || fail "$mode $warning: exit status is not 1"
		grep 'rejected its settings' "$work/err" > /dev/null ||
			fail "$mode $warning: not reported"
		grep -F -e "$warning" "$work/err" > /dev/null ||
			fail "$mode $warning: no log"
		pid=$(cat "$work/alacritty.pid")
		i=0
		while kill -0 "$pid" 2> /dev/null
		do
			i=$((i + 1))
			test $i -le 20 || fail "$mode $warning: terminal still running"
			sleep 0.1
		done
		if test "$mode" = -w && test -n "$(ls -A "$work/tmp-w")"
		then
			fail "$mode $warning: remote copy left behind"
		fi
	done
done

RPSHOW_SSH=$work/no-such-ssh "$rpshow" "$work/a" 2> "$work/err"
test $? -eq 127 || fail 'missing ssh: exit status is not 127'
exit 0
