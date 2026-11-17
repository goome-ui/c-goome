#!/usr/bin/env bash
set -u
eng=${1:?ibus-libpinyin|ibus-mozc|ibus-anthy|fcitx5-pinyin|fcitx5-mozc|fcitx5-anthy}
source "$(dirname "$0")/lib.sh"
cell="ime:linux-x11:$eng"
S=$VERIFY_OUT/imex-$eng
rm -rf "$S"
mkdir -p "$S/home" "$S/rt"
chmod 700 "$S/rt"
export HOME=$S/home XDG_RUNTIME_DIR=$S/rt XDG_CONFIG_HOME=$S/home/.config XDG_CACHE_HOME=$S/home/.cache XDG_DATA_HOME=$S/home/.local/share
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
export LANG=C.UTF-8 LC_ALL=C.UTF-8
unset WAYLAND_DISPLAY XDG_CURRENT_DESKTOP GOOME_BACKEND
export XDG_SESSION_TYPE=x11
if [ -z "${IME_INNER:-}" ]; then
  export IME_INNER=1
  exec env -u DBUS_SESSION_BUS_ADDRESS dbus-run-session -- bash "$0" "$@"
fi
VER=
bail() { emit "$cell" 0 fail "$1" "$VER"; exit 0; }
pids=()
trap 'kill "${pids[@]}" 2>/dev/null' EXIT
build_lib || bail "lib build failed"
build_prog "$ROOT/tests/ime_ui.c" || bail "ime_ui build failed"
d=":$((RANDOM % 300 + 300))"
Xvfb "$d" -screen 0 1024x768x24 -nolisten tcp >"$S/xvfb.log" 2>&1 &
pids+=($!)
for i in $(seq 50); do [ -e "/tmp/.X11-unix/X${d#:}" ] && break; sleep 0.1; done
export DISPLAY=$d
case $eng in
ibus-libpinyin) fam=ibus; engine=libpinyin; keys="n i h a o"; want="你好"; commit="space" ;;
ibus-mozc) fam=ibus; engine=mozc-jp; keys="n i h o n g o"; want="にほんご|日本語"; commit="Return" ;;
ibus-anthy) fam=ibus; engine=anthy; keys="n i h o n g o"; want="にほんご|日本語"; commit="Return" ;;
fcitx5-pinyin) fam=fcitx5; engine=pinyin; keys="n i h a o"; want="你好"; commit="space" ;;
fcitx5-mozc) fam=fcitx5; engine=mozc; keys="n i h o n g o"; want="にほんご|日本語"; commit="Return" ;;
fcitx5-anthy) fam=fcitx5; engine=anthy; keys="n i h o n g o"; want="にほんご|日本語"; commit="Return" ;;
*) bail "unknown engine" ;;
esac
if [ "$fam" = ibus ]; then
  command -v ibus-daemon >/dev/null || bail "ibus-daemon missing: $(tailof "$VERIFY_OUT/deps-$eng.log" 6)"
  VER=$(ibus version 2>&1 | head -1)
  export IBUS_ADDRESS=unix:path=$S/ibus-sock
  ibus-daemon -x -p disable -E disable -c disable -a "$IBUS_ADDRESS" >"$S/ibus.log" 2>&1 &
  pids+=($!)
  sleep 3
  ibus list-engine >"$S/engines.log" 2>&1
  for t in 1 2 3; do ibus engine "$engine" >>"$S/ibus.log" 2>&1 && break; sleep 5; done
  sleep 1
  echo "active engine: $(ibus engine 2>&1)" >>"$S/ibus.log"
  im=@im=ibus
else
  command -v fcitx5 >/dev/null || bail "fcitx5 missing: $(tailof "$VERIFY_OUT/deps-$eng.log" 6)"
  VER=$(fcitx5 --version 2>&1 | head -1)
  mkdir -p "$XDG_CONFIG_HOME/fcitx5"
  printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=%s\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=%s\nLayout=\n\n[GroupOrder]\n0=Default\n' "$engine" "$engine" >"$XDG_CONFIG_HOME/fcitx5/profile"
  fcitx5 >"$S/fcitx5.log" 2>&1 &
  pids+=($!)
  sleep 10
  fcitx5-remote -s "$engine" >>"$S/fcitx5.log" 2>&1
  sleep 1
  echo "fcitx5-remote now: $(fcitx5-remote -n 2>&1)" >>"$S/fcitx5.log"
  im=@im=fcitx
  xprop -root XIM_SERVERS >>"$S/fcitx5.log" 2>&1
fi
env XMODIFIERS=$im GOOME_BACKEND=x11 "$VB/ime_ui" >"$S/ime.log" 2>"$S/ime.err" &
pids+=($!)
waitfor "$S/ime.log" '^ime rc' 60
sleep 1
xdotool search --name '^goome ime$' windowfocus 2>/dev/null
sleep 0.5
if [ "$fam" = ibus ] && [ "$engine" != libpinyin ]; then xdotool key Zenkaku_Hankaku; sleep 0.5; fi
for k in $keys; do xdotool key "$k"; sleep 0.25; done
sleep 1
cp "$S/ime.log" "$S/ime-before-commit.log"
xdotool key "$commit"
sleep 1.5
xdotool key Return
sleep 1
rc=$(grep '^ime rc' "$S/ime.log" | head -1)
committed=$(grep -E '^text ' "$S/ime.log" | tr '\n' ';')
pre=$(grep -E '^preedit' "$S/ime.log" | head -4 | tr '\n' ';')
if grep -E '^(text|buf) ' "$S/ime.log" | grep -qE "$want"; then
  emit "$cell" 1 pass "$VER; engine $engine; goome '$rc'; preedit events: $pre committed: $committed buf: $(grep '^buf' "$S/ime.log" | tail -1)" "$VER"
else
  emit "$cell" 1 fail "$VER; engine $engine; wanted $want; XIM_SERVERS: $(grep XIM_SERVERS "$S/fcitx5.log" "$S/ibus.log" 2>/dev/null | head -1); goome '$rc'; preedit: $pre committed: '$committed'; ime.log tail: $(tailof "$S/ime.log" 8) | im log: $(tailof "$S/ibus.log" 6)$(tailof "$S/fcitx5.log" 6) engines: $(head -c 300 "$S/engines.log" 2>/dev/null | tr '\n' ' ') | err: $(tailof "$S/ime.err" 4)" "$VER"
fi
exit 0
