#!/bin/sh
# Check that a PDF, found by its name or by its first bytes, is shown in
# zathura with the colour settings from the local zathurarc, that --light,
# --raw and a missing zathura work, and that the remote copy is removed.
#
# Copyright (C) 2026 mb6ockatf
# This file is free software; see COPYING for copying conditions.

. "$top_srcdir/tests/init.sh"

mkdir "$work/src" "$work/home/.config" "$work/home/.config/zathura"
printf '%%PDF-1.4\nfake pdf body\n' > "$work/src/paper.pdf"
config=$work/home/.config/zathura/zathurarc
cat > "$config" <<'END'
set window-title-basename "true"
set default-bg                  rgba(0,0,0,1)         # bg
set recolor-lightcolor          rgba(0,0,0,1)         # bg
set recolor-darkcolor           rgba(192,202,245,1)   # fg
set recolor true
set default-fg "$(touch pwned)"
map <Button1> exec "touch pwned"
include other
END
expected_config='set guioptions ""
set recolor true
map [normal] <Button3> quit
map [fullscreen] <Button3> quit
set default-bg "rgba(0,0,0,1)"
set recolor-lightcolor "rgba(0,0,0,1)"
set recolor-darkcolor "rgba(192,202,245,1)"
set recolor "true"'

# check_zathura NAME: the PDF arrived intact under NAME, with the right
# options, and nothing was left behind.
check_zathura () {
	shown=$(cat "$work/zathura.file")
	test "${shown##*/}" = "$1" || fail "$1: name not kept: $shown"
	for want in --mode=fullscreen --config-dir=* --data-dir=* --cache-dir=*
	do
		grep -x -e "$(printf '%s' "$want" | sed 's/\*/.*/')" \
			"$work/zathura.args" > /dev/null ||
			fail "$1: zathura lacks $want"
	done
	test "$(cat "$work/zathura.display")" = ':0' || fail "$1: display"
	leftover_tmp && fail "$1: remote copy left behind"
	test -e pwned && fail "$1: command from zathurarc was run"
	return 0
}

for name in paper.pdf 'with space.PDF' "it's.pdf" '$(touch pwned).pdf'
do
	test "$name" = paper.pdf || cp "$work/src/paper.pdf" "$work/src/$name"
	rm -f "$work/zathura.args" "$work/alacritty.args"
	"$rpshow" -w "$work/src/$name" 2> "$work/err" ||
		fail "$name: exit status $?"
	test -s "$work/err" && fail "$name: unexpected message: $(cat "$work/err")"
	test -e "$work/alacritty.args" && fail "$name: alacritty started"
	cmp "$work/src/$name" "$work/zathura.content" ||
		fail "$name: content differs"
	test "$(cat "$work/zathura.config")" = "$expected_config" ||
		fail "$name: zathurarc: $(cat "$work/zathura.config")"
	check_zathura "$name"
done

# A PDF without a .pdf name is found by its first bytes and gets one.
cp "$work/src/paper.pdf" "$work/src/download"
"$rpshow" -w "$work/src/download" || fail 'magic: exit status'
check_zathura download.pdf
cmp "$work/src/download" "$work/zathura.content" || fail 'magic: content'

# Text that only mentions %PDF- later is not a PDF.
printf 'see %%PDF-1.4\n' > "$work/src/notes.txt"
rm -f "$work/zathura.args"
"$rpshow" -w "$work/src/notes.txt" || fail 'txt: exit status'
test -e "$work/zathura.args" && fail 'txt: shown in zathura'

# --light turns the recolouring off after the zathurarc settings.
"$rpshow" -w --light "$work/src/paper.pdf" || fail 'light: exit status'
test "$(sed "/^$/d" "$work/zathura.config" | tail -n 1)" = 'set recolor "false"' ||
	fail 'light: recolor not turned off'

# $XDG_CONFIG_HOME wins over ~/.config; without a zathurarc the PDF is
# still recoloured.
mkdir "$work/xdg"
XDG_CONFIG_HOME=$work/xdg "$rpshow" -w "$work/src/paper.pdf" ||
	fail 'xdg: exit status'
grep -F rgba "$work/zathura.config" > /dev/null && fail 'xdg: ~/.config read'
grep -Fx 'set recolor true' "$work/zathura.config" > /dev/null ||
	fail 'xdg: not recoloured'
mkdir "$work/xdg/zathura"
printf 'set default-bg "#123456"\n' > "$work/xdg/zathura/zathurarc"
XDG_CONFIG_HOME=$work/xdg "$rpshow" -w "$work/src/paper.pdf" ||
	fail 'xdg: exit status'
grep -Fx 'set default-bg "#123456"' "$work/zathura.config" > /dev/null ||
	fail 'xdg: zathurarc not read'

# An unreadable zathurarc is an error, before ssh runs.
if test "$(id -u)" != 0
then
	chmod 0 "$config"
	rm -f "$work/ssh.args"
	"$rpshow" -w "$work/src/paper.pdf" 2> "$work/err" &&
		fail 'unreadable: succeeded'
	grep -F zathurarc "$work/err" > /dev/null || fail 'unreadable: no message'
	test -e "$work/ssh.args" && fail 'unreadable: ssh was run'
	chmod 644 "$config"
fi

# --raw shows the PDF as text in less.
"$rpshow" -w --raw "$work/src/paper.pdf" || fail 'raw: exit status'
test "$(sed '1,/^-e$/d' "$work/alacritty.args" | sed -n 1p)" = less ||
	fail 'raw: not shown in less'

# Without zathura the PDF is shown as text, with a note.
# The PATH holds only what the remote script and the fakes need, so a
# real zathura on this machine cannot be found.
mkdir "$work/no-zathura"
ln -s "$top_srcdir/tests/bin/alacritty" "$work/no-zathura/alacritty"
for tool in sh cat cp mktemp rm less setsid sleep grep sed uname
do
	tool_path=$(command -v "$tool") || fail "no zathura: $tool not found"
	ln -s "$tool_path" "$work/no-zathura/$tool"
done
PATH=$work/no-zathura "$rpshow" -w "$work/src/paper.pdf" \
	2> "$work/err" || fail 'no zathura: exit status'
grep -F 'zathura not found; showing the PDF as text' "$work/err" \
	> /dev/null || fail 'no zathura: no note'
test "$(sed '1,/^-e$/d' "$work/alacritty.args" | sed -n 1p)" = less ||
	fail 'no zathura: not shown in less'

# A failing zathura is reported; -v passes its warnings on.
FAKE_ZATHURA_STATUS=3 "$rpshow" -w "$work/src/paper.pdf" 2> "$work/err" &&
	fail 'failing zathura: succeeded'
grep -F 'cannot open display' "$work/err" > /dev/null ||
	fail 'failing zathura: log not shown'
FAKE_ZATHURA_WARN='warning: Unknown option: recolor-reverse-video' \
	"$rpshow" -w -v "$work/src/paper.pdf" 2> "$work/err" ||
	fail 'warning: exit status'
grep -F 'rpshow: zathura: Unknown option: recolor-reverse-video' \
	"$work/err" > /dev/null || fail 'warning: not reported'
exit 0
