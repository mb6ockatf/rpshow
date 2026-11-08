#!/bin/sh
# Check that a Markdown file is rendered with rich before less shows it,
# that --raw and a missing rich module show the source instead, and that
# other files are not rendered.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

mkdir "$work/src"
printf '# Title\n\nSome *emphasis* and `code`.\n\n- item\n' > "$work/src/notes.md"
printf 'plain\n' > "$work/src/notes.txt"

# terminal_command: print the command alacritty was told to run, one
# word per line.
terminal_command () {
	sed '1,/^-e$/d' "$work/alacritty.args"
}

for name in notes.md 'with space.MD' "it's.markdown"
do
	cp "$work/src/notes.md" "$work/src/$name"
	"$rpshow" -w "$work/src/$name" 2> "$work/err" ||
		fail "$name: exit status $?"
	test -s "$work/err" && fail "$name: unexpected message: $(cat "$work/err")"
	cmp "$work/src/$name" "$work/alacritty.content" ||
		fail "$name: content differs"
	terminal_command > "$work/command"
	test "$(sed -n 1p "$work/command")" = sh || fail "$name: not run by sh"
	test "$(sed -n 2p "$work/command")" = -c || fail "$name: no sh -c"
	sed -n 3p "$work/command" | grep -F 'python3 -m rich.markdown -c "$1" | less -R' \
		> /dev/null || fail "$name: not rendered with rich"
	shown=$(sed -n 5p "$work/command")
	test "${shown##*/}" = "$name" || fail "$name: name not kept: $shown"
	leftover_tmp && fail "$name: remote copy left behind"
done

# The rendering command works: run it with a real python3 that has rich,
# with cat standing in for less.
rich_python=$(PATH=$real_path command -v python3) &&
	"$rich_python" -c 'import rich.markdown' > /dev/null 2>&1 &&
{
	mkdir "$work/render-bin"
	ln -s "$rich_python" "$work/render-bin/python3"
	printf '#!/bin/sh\ncat\n' > "$work/render-bin/less"
	chmod +x "$work/render-bin/less"
	"$rpshow" -w "$work/src/notes.md" || fail 'render: exit status'
	terminal_command > "$work/command"
	script=$(sed -n 3p "$work/command")
	PATH=$work/render-bin:$real_path COLUMNS=60 \
		sh -c "$script" rpshow "$work/alacritty.content" \
		> "$work/rendered" 2> "$work/err" < /dev/null ||
		fail "render: rich failed: $(cat "$work/err")"
	grep -F Title "$work/rendered" > /dev/null || fail 'render: no title'
	grep '^#' "$work/rendered" > /dev/null && fail 'render: source shown'
	esc=$(printf '\033')
	grep -F "$esc[" "$work/rendered" > /dev/null || fail 'render: no colors'
}

# --raw shows the source.
"$rpshow" -w --raw "$work/src/notes.md" || fail 'raw: exit status'
test "$(terminal_command | sed -n 1p)" = less || fail 'raw: rendered'

# Without the rich module the source is shown, with a note.
FAKE_PYTHON_NO_RICH=1 "$rpshow" -w "$work/src/notes.md" 2> "$work/err" ||
	fail 'no rich: exit status'
test "$(terminal_command | sed -n 1p)" = less || fail 'no rich: rendered'
grep 'rich module not found; showing the Markdown source' "$work/err" \
	> /dev/null || fail 'no rich: no note'

# Other files are not rendered.
"$rpshow" -w "$work/src/notes.txt" || fail 'txt: exit status'
test "$(terminal_command | sed -n 1p)" = less || fail 'txt: rendered'
exit 0
