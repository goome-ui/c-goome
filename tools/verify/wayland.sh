#!/usr/bin/env bash
set -u
comp=${1:?compositor}
source "$(dirname "$0")/lib.sh"
distro=${DISTRO:-ubuntu}
cell="wayland:$comp:$distro"
S=$VERIFY_OUT/wl-$comp-$distro
rm -rf "$S"
mkdir -p "$S"
RT=$S/rt
mkdir -p "$RT" "$S/home"
chmod 700 "$RT"
export HOME=$S/home XDG_RUNTIME_DIR=$RT XDG_CONFIG_HOME=$S/home/.config XDG_CACHE_HOME=$S/home/.cache XDG_DATA_HOME=$S/home/.local/share
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME"
unset DISPLAY WAYLAND_DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE GOOME_BACKEND XDG_CURRENT_DESKTOP
export XDG_SESSION_TYPE=wayland
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
B=$VB
mkdir -p "$B"
INPUT=wlr
XD=
XNAME=
CPID=
pids=()
finish() { kill "${pids[@]}" $CPID 2>/dev/null; sleep 0.5; kill -9 "${pids[@]}" $CPID 2>/dev/null; }
trap finish EXIT
bail() {
  emit "$cell:open" 0 fail "$1" "${VER:-}"
  emit "$cell:hit" 0 fail "not run: $1" "${VER:-}"
  emit "$cell:through" 0 fail "not run: $1" "${VER:-}"
  exit 0
}

bin=$comp
case $comp in
weston-*) bin=weston ;;
kwin) bin=kwin_wayland ;;
hyprland) bin=Hyprland ;;
esac
if [ "$(id -u)" = 0 ]; then setcap -r "$(command -v "$bin")" 2>/dev/null; fi
if ! command -v "$bin" >/dev/null; then bail "$bin is not installed on $distro: $(tailof "$S/../deps-$comp-$distro.log" 6)"; fi
VER=$("$bin" --version 2>&1 | head -1)
[ -n "$VER" ] || VER=$bin

gen_protocols() {
  local x
  x=$(ls /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml 2>/dev/null | head -1)
  [ -n "$x" ] || return 1
  wayland-scanner client-header "$x" "$S/xdg-shell-client-protocol.h" && wayland-scanner private-code "$x" "$S/xdg-shell-protocol.c" || return 1
  cc -std=gnu99 -O1 -w -I"$S" -o "$B/wl_under" "$ROOT/tools/verify/wl_under.c" "$S/xdg-shell-protocol.c" -lwayland-client || return 1
  cc -O1 -w -o "$B/wl_vptr" "$ROOT/tests/wl_vptr.c" -lwayland-client || return 1
  local f
  f=$(ls /usr/share/plasma-wayland-protocols/fake-input.xml 2>/dev/null | head -1)
  if [ -n "$f" ]; then
    wayland-scanner client-header "$f" "$S/fake-input-client-protocol.h" && wayland-scanner private-code "$f" "$S/fake-input-protocol.c" &&
      cc -std=gnu99 -O1 -w -I"$S" -o "$B/kw_input" "$ROOT/tools/verify/kw_input.c" "$S/fake-input-protocol.c" -lwayland-client || echo "kw_input build failed"
  fi
  return 0
}
build_lib || bail "goome lib build failed"
build_prog "$ROOT/tests/wl_watch.c" || bail "wl_watch build failed"
if ! gen_protocols >"$S/protocols.log" 2>&1; then bail "helper clients did not build: $(tailof "$S/protocols.log" 6)"; fi

wait_socket() {
  local i s
  for i in $(seq "${1:-150}"); do
    s=$(ls "$RT" 2>/dev/null | grep -E '^(wayland-[0-9]+|wl-[a-z]+)$' | head -1)
    [ -n "$s" ] && [ -S "$RT/$s" ] && { export WAYLAND_DISPLAY=$s; return 0; }
    kill -0 "$CPID" 2>/dev/null || return 1
    sleep 0.1
  done
  return 1
}
start_xvfb() {
  local n=$((RANDOM % 300 + 200))
  Xvfb ":$n" -screen 0 1920x1080x24 +extension GLX -nolisten tcp >"$S/xvfb.log" 2>&1 &
  pids+=($!)
  XD=":$n"
  local i
  for i in $(seq 50); do [ -e "/tmp/.X11-unix/X$n" ] && break; sleep 0.1; done
}

APP=
case $comp in
sway)
  printf 'xwayland enable\ndefault_border none\n' >"$S/sway.cfg"
  export XDG_CURRENT_DESKTOP=sway
  sway -c "$S/sway.cfg" >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
labwc)
  export XDG_CURRENT_DESKTOP=labwc:wlroots
  labwc >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
wayfire)
  export XDG_CURRENT_DESKTOP=Wayfire
  printf '[core]\nplugins = move resize place window-rules\nxwayland = true\n' >"$S/wayfire.ini"
  wayfire -c "$S/wayfire.ini" >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
river)
  export XDG_CURRENT_DESKTOP=river
  mkdir -p "$XDG_CONFIG_HOME/river"
  printf '#!/bin/sh\nrivertile -view-padding 0 -outer-padding 0 &\nriver-classic -h >/dev/null 2>&1\nriverctl default-layout rivertile\nexec sleep 3600\n' >"$XDG_CONFIG_HOME/river/init"
  chmod +x "$XDG_CONFIG_HOME/river/init"
  river >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
cage)
  export XDG_CURRENT_DESKTOP=cage
  cage -- "$B/wl_under" >"$S/under.log" 2>"$S/comp.log" &
  CPID=$!
  APP=cage
  ;;
hyprland)
  export XDG_CURRENT_DESKTOP=Hyprland HYPRLAND_HEADLESS_ONLY=1 AQ_NO_MODIFIERS=1
  printf 'misc {\n disable_hyprland_logo = true\n}\n' >"$S/hypr.conf"
  Hyprland -c "$S/hypr.conf" >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
weston-desktop | weston-kiosk | weston-headless)
  export XDG_CURRENT_DESKTOP=weston
  shell=desktop
  [ "$comp" = weston-kiosk ] && shell=kiosk
  if [ "$comp" = weston-headless ]; then
    INPUT=none
    weston --backend=headless --renderer=pixman --socket=wl-weston --shell=$shell --width=1280 --height=720 --xwayland >"$S/comp.log" 2>&1 &
    CPID=$!
  else
    start_xvfb
    INPUT=x11
    XNAME=Weston
    DISPLAY=$XD weston --backend=x11 --renderer=pixman --socket=wl-weston --shell=$shell --width=1280 --height=720 --xwayland >"$S/comp.log" 2>&1 &
    CPID=$!
    sleep 1
    if ! kill -0 $CPID 2>/dev/null; then
      DISPLAY=$XD weston --backend=x11-backend.so --use-pixman --socket=wl-weston --shell=${shell}-shell.so --width=1280 --height=720 >"$S/comp2.log" 2>&1 &
      CPID=$!
    fi
  fi
  ;;
niri)
  export XDG_CURRENT_DESKTOP=niri
  start_xvfb
  INPUT=x11
  XNAME=niri
  mkdir -p "$XDG_CONFIG_HOME/niri"
  printf 'hotkey-overlay {\n    skip-at-startup\n}\n' >"$XDG_CONFIG_HOME/niri/config.kdl"
  DISPLAY=$XD LIBGL_ALWAYS_SOFTWARE=1 niri >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
kwin)
  export XDG_CURRENT_DESKTOP=KDE KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 KWIN_COMPOSE=Q QT_QPA_PLATFORM=offscreen
  INPUT=kwin
  dbus-run-session -- kwin_wayland --virtual --no-lockscreen --no-global-shortcuts --socket wl-kwin --width 1280 --height 720 --xwayland >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
mutter)
  export XDG_CURRENT_DESKTOP=GNOME
  INPUT=mutter
  command -v pipewire >/dev/null && { pipewire >"$S/pipewire.log" 2>&1 & pids+=($!); sleep 2; }
  dbus-run-session -- bash -c 'echo "$DBUS_SESSION_BUS_ADDRESS" > "$HOME/bus"; exec mutter --headless --wayland --virtual-monitor 1280x720 --wayland-display wl-mutter' >"$S/comp.log" 2>&1 &
  CPID=$!
  ;;
*) bail "unknown compositor $comp" ;;
esac

if ! wait_socket 200; then bail "compositor never gave a wayland socket: $(tailof "$S/comp.log" 12) $(tailof "$S/comp2.log" 6)"; fi
sleep 2
if [ "$comp" != mutter ]; then
  xs=$(sed -n "s/.*xserver listening on display \(:[0-9]*\).*/\1/p" "$S/comp.log" | head -1)
  [ -n "$xs" ] || xs=$(ls -t /tmp/.X11-unix/ 2>/dev/null | grep -E "^X[0-9]+$" | sed "s/^X/:/" | grep -vx "${XD:-none}" | head -1)
  [ -n "$xs" ] && export DISPLAY=$xs
  echo "goome DISPLAY=${DISPLAY:-unset}" >>"$S/info.log"
fi
if ! kill -0 $CPID 2>/dev/null; then bail "compositor exited: $(tailof "$S/comp.log" 12)"; fi

case $comp in
mutter)
  for i in $(seq 100); do grep -q 'Using public X11 display' "$S/comp.log" 2>/dev/null && break; sleep 0.1; done
  xdpy=$(sed -n 's/.*Using public X11 display \(:[0-9]*\).*/\1/p' "$S/comp.log" | head -1)
  [ -n "$xdpy" ] && export DISPLAY=$xdpy XAUTHORITY=$(ls "$RT"/.mutter-Xwaylandauth.* 2>/dev/null | head -1)
  export DBUS_SESSION_BUS_ADDRESS=$(cat "$HOME/bus" 2>/dev/null)
  ;;
kwin)
  dbus_addr=
  ;;
esac

if [ "$INPUT" = x11 ]; then
  geo=$(DISPLAY=$XD xdotool search --onlyvisible --name "$XNAME" 2>/dev/null | head -1)
  eval "$(DISPLAY=$XD xdotool getwindowgeometry --shell "${geo:-0}" 2>/dev/null)"
  OX=${X:-0}
  OY=${Y:-0}
  echo "nested window at $OX,$OY" >>"$S/info.log"
fi

if [ "$APP" != cage ]; then
  "$B/wl_under" >"$S/under.log" 2>&1 &
  pids+=($!)
fi
if ! waitfor "$S/under.log" '^under ready' 80; then
  bail "under client never got a mapped window: $(tailof "$S/under.log" 6) | comp: $(tailof "$S/comp.log" 8)"
fi

if [ "$INPUT" = mutter ]; then
  "$PY" -I "$ROOT/tests/gnome_input.py" "run:$S/watch.log:$B/wl_watch $S/state" "wait:$S/watch.log:^monitors" sleep:6 move:600,500 click sleep:0.6 move:220,150 click sleep:0.8 >"$S/inject.log" 2>&1
else
  mkdir -p "$S/state"
  "$B/wl_watch" "$S/state" >"$S/watch.log" 2>&1 &
  pids+=($!)
  waitfor "$S/watch.log" '^(monitors|open failed)' 80
  sleep 1.5
fi

backend=$(sed -n 's/^backend: \(.*\) caps.*/\1/p' "$S/watch.log" | head -1)
if ! grep -q '^monitors [1-9]' "$S/watch.log"; then
  bail "goome did not open: $(tailof "$S/watch.log" 6) | comp: $(tailof "$S/comp.log" 6)"
fi
emit "$cell:open" 1 pass "goome backend '$backend'; $(grep '^mon 0' "$S/watch.log" | head -1); compositor $VER" "$VER"
read -r MW MH <<<"$(sed -n 's/^mon 0 .* \([0-9]*\)x\([0-9]*\)+[-0-9]*+[-0-9]* scale.*/\1 \2/p' "$S/watch.log" | head -1)"
UW=$(sed -n 's/^under ready \([0-9]*\)x.*/\1/p' "$S/under.log" | tail -1)
UH=$(sed -n 's/^under ready [0-9]*x\([0-9]*\)/\1/p' "$S/under.log" | tail -1)

click() {
  case $INPUT in
  wlr) "$B/wl_vptr" "$MW" "$MH" c "$1" "$2" >>"$S/inject.log" 2>&1 ;;
  x11)
    DISPLAY=$XD xdotool mousemove $((OX + $1)) $((OY + $2)) >>"$S/inject.log" 2>&1
    sleep 0.4
    DISPLAY=$XD xdotool click 1 >>"$S/inject.log" 2>&1
    sleep 0.4
    ;;
  kwin) "$B/kw_input" "$MW" "$MH" c "$1" "$2" >>"$S/inject.log" 2>&1 ;;
  esac
}
if [ "$INPUT" = none ]; then
  emit "$cell:hit" 0 fail "weston headless backend has no input device and no virtual pointer protocol, so no click can be injected (under client ready ${UW}x${UH})" "$VER"
  emit "$cell:through" 0 fail "same: no way to inject a click into the headless backend" "$VER"
  exit 0
fi
if [ "$INPUT" != mutter ]; then
  click 600 500
  sleep 0.5
  click 220 150
  sleep 0.8
fi

if grep -q '^down 1 22[0-9],15[0-9]' "$S/watch.log"; then
  emit "$cell:hit" 1 pass "click inside the hit rect reached goome: $(grep '^down' "$S/watch.log" | head -2 | tr '\n' ';') input via $INPUT" "$VER"
else
  emit "$cell:hit" 1 fail "no down event at 220,150 in goome log ($(grep -c '^down' "$S/watch.log") downs; motions $(grep -c '^motion' "$S/watch.log")); input via $INPUT; inject: $(tailof "$S/inject.log" 4) watch: $(tailof "$S/watch.log" 6)" "$VER"
fi
if grep -q '^under down 272 60[0-9],50[0-9]' "$S/under.log" && ! grep -q '^down 1 60[0-9],50[0-9]' "$S/watch.log"; then
  inside=no
  grep -q '^under down 272 22[0-9],15[0-9]' "$S/under.log" && inside=yes
  emit "$cell:through" 1 pass "click outside the rect reached the window underneath ($(grep '^under down' "$S/under.log" | head -2 | tr '\n' ';')) and not goome; click inside the rect also reached the window below: $inside; input via $INPUT" "$VER"
else
  emit "$cell:through" 1 fail "under client log: $(tailof "$S/under.log" 6) | goome downs: $(grep '^down' "$S/watch.log" | tr '\n' ';') | input via $INPUT inject: $(tailof "$S/inject.log" 4)" "$VER"
fi
exit 0
