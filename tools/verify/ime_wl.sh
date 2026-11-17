#!/usr/bin/env bash
set -u
eng=${1:?fcitx5-pinyin|ibus-libpinyin|fcitx5-mozc}
source "$(dirname "$0")/lib.sh"
cell="ime:linux-wayland-sway:$eng"
S=$VERIFY_OUT/imewl-$eng
rm -rf "$S"
mkdir -p "$S/home" "$S/rt"
chmod 700 "$S/rt"
export HOME=$S/home XDG_RUNTIME_DIR=$S/rt XDG_CONFIG_HOME=$S/home/.config XDG_CACHE_HOME=$S/home/.cache XDG_DATA_HOME=$S/home/.local/share
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
export LANG=C.UTF-8 LC_ALL=C.UTF-8
unset DISPLAY WAYLAND_DISPLAY GOOME_BACKEND
export XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=sway WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
if [ -z "${IME_INNER:-}" ]; then
  export IME_INNER=1
  exec env -u DBUS_SESSION_BUS_ADDRESS dbus-run-session -- bash "$0" "$@"
fi
VER=
bail() { emit "$cell" 0 fail "$1" "$VER"; exit 0; }
pids=()
trap 'kill "${pids[@]}" 2>/dev/null' EXIT
command -v sway >/dev/null || bail "sway missing"
command -v wtype >/dev/null || bail "wtype missing"
build_lib || bail "lib build failed"
build_prog "$ROOT/tests/ime_ui.c" || bail "ime_ui build failed"
printf 'default_border none\nxwayland disable\n' >"$S/sway.cfg"
sway -c "$S/sway.cfg" >"$S/sway.log" 2>&1 &
pids+=($!)
for i in $(seq 100); do s=$(ls "$XDG_RUNTIME_DIR" | grep -E '^wayland-[0-9]+$' | head -1); [ -n "$s" ] && break; sleep 0.1; done
[ -n "${s:-}" ] || bail "sway gave no socket: $(tailof "$S/sway.log" 8)"
export WAYLAND_DISPLAY=$s
sleep 2
case $eng in
fcitx5-pinyin) engine=pinyin; keys="nihao"; want="你好" ;;
fcitx5-mozc) engine=mozc; keys="nihongo"; want="にほんご|日本語" ;;
ibus-libpinyin) engine=libpinyin; keys="nihao"; want="你好" ;;
*) bail "unknown engine" ;;
esac
case $eng in
fcitx5-*)
  command -v fcitx5 >/dev/null || bail "fcitx5 missing: $(tailof "$VERIFY_OUT/deps-$eng.log" 6)"
  VER=$(fcitx5 --version 2>&1 | head -1)
  mkdir -p "$XDG_CONFIG_HOME/fcitx5"
  printf '[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=%s\n\n[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n[Groups/0/Items/1]\nName=%s\nLayout=\n\n[GroupOrder]\n0=Default\n' "$engine" "$engine" >"$XDG_CONFIG_HOME/fcitx5/profile"
  fcitx5 --verbose '*=5' >"$S/im.log" 2>&1 &
  pids+=($!)
  sleep 5
  fcitx5-remote -s "$engine" >>"$S/im.log" 2>&1
  ;;
ibus-*)
  command -v ibus-daemon >/dev/null || bail "ibus-daemon missing: $(tailof "$VERIFY_OUT/deps-$eng.log" 6)"
  VER=$(ibus version 2>&1 | head -1)
  ibus-daemon -r -x -p disable -E disable -c disable >"$S/im.log" 2>&1 &
  pids+=($!)
  sleep 4
  ibus engine "$engine" >>"$S/im.log" 2>&1
  ibus-wayland >"$S/ibus-wayland.log" 2>&1 &
  pids+=($!)
  sleep 2
  ;;
esac
sleep 1
GOOME_BACKEND=wayland "$VB/ime_ui" >"$S/ime.log" 2>"$S/ime.err" &
pids+=($!)
waitfor "$S/ime.log" '^ime rc' 80
sleep 2
rcline=$(grep '^ime rc' "$S/ime.log" | head -1)
wtype -s 300 "$keys"
sleep 1.5
wtype -k space
sleep 1.5
wtype -k Return
sleep 1
pre=$(grep -E '^preedit' "$S/ime.log" | grep -v "''" | head -4 | tr '\n' ';')
committed=$(grep -E '^text ' "$S/ime.log" | tr '\n' ';')
if grep -qE '^preedit' "$S/ime.log" && grep -E '^(text|buf) ' "$S/ime.log" | grep -qE "$want"; then
  emit "$cell" 1 pass "sway; $VER; goome '$rcline'; preedit events: $pre committed: $committed" "$VER"
else
  emit "$cell" 1 fail "sway; $VER; wanted $want and preedit; goome '$rcline'; preedit: '$pre' committed: '$committed'; ime.log: $(tailof "$S/ime.log" 8) | im: $(tailof "$S/im.log" 8) | err $(tailof "$S/ime.err" 4)" "$VER"
fi
exit 0
