#!/bin/sh
# Check that the file reaches the (fake) remote terminal intact, with the
# right terminal and less settings, and that the remote copy is removed.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

mkdir "$work/src"
for name in plain.txt 'with space' "it's" '$(touch pwned)' '-dash' 'a"b\c'
do
	file=$work/src/$name
	printf 'line one\n%s\n\tend\n' "$name" > "$file"
	rm -f "$work/alacritty.args"
	RPSHOW_LESS="-R --bogus-option --mouse -i" \
		"$rpshow" -w -H 'my host' -d ':7' "$file" ||
		fail "$name: exit status $?"
	test -e pwned && fail "$name: command substitution was run"
	cmp "$file" "$work/alacritty.content" || fail "$name: content differs"
	test "$(cat "$work/ssh.host")" = 'my host' || fail "$name: host"
	test "$(cat "$work/alacritty.display")" = ':7' || fail "$name: display"
	test "$(cat "$work/alacritty.less")" = '-R -i' || fail "$name: LESS"
	shown=$(cat "$work/alacritty.file")
	test "${shown##*/}" = "$name" || fail "$name: name not kept: $shown"
	for want in rpshow 'window.startup_mode="Fullscreen"' window.opacity=1.0 \
		'mouse.bindings=[{mouse="Right",action="Quit"}]' less
	do
		grep -Fx -e "$want" "$work/alacritty.args" > /dev/null ||
			fail "$name: alacritty lacks $want"
	done
	leftover_tmp && fail "$name: remote copy left behind"
done

# $LESS is the fallback for $RPSHOW_LESS; -v reports dropped options.
LESS='-Q --bogus-option' "$rpshow" -v -w "$work/src/plain.txt" \
	2> "$work/err" || fail 'LESS: exit status'
test "$(cat "$work/alacritty.less")" = '-Q' || fail 'LESS not used'
test "$(cat "$work/ssh.host")" = pi || fail 'default host'
test "$(cat "$work/alacritty.display")" = ':0' || fail 'default display'
grep 'dropping less option --bogus-option' "$work/err" > /dev/null ||
	fail '-v did not report the dropped option'

# RPSHOW_HOST sets the default host.
RPSHOW_HOST=elsewhere "$rpshow" -w "$work/src/plain.txt" ||
	fail 'RPSHOW_HOST: exit status'
test "$(cat "$work/ssh.host")" = elsewhere || fail 'RPSHOW_HOST not used'

# Without -w rpshow returns, and the remote copy is removed later.
rm -f "$work/alacritty.content"
"$rpshow" "$work/src/plain.txt" || fail 'detached: exit status'
i=0
while leftover_tmp
do
	i=$((i + 1))
	test $i -le 50 || fail 'detached: remote copy left behind'
	sleep 0.1
done
cmp "$work/src/plain.txt" "$work/alacritty.content" ||
	fail 'detached: content differs'
exit 0
