#!/usr/bin/env bash
set -u
wm=${1:?wm}
source "$(dirname "$0")/lib.sh"
cell="x11:$wm"
S=$VERIFY_OUT/x11-$wm
rm -rf "$S"
mkdir -p "$S"
export TMPDIR=$S HOME=$S
unset WAYLAND_DISPLAY XDG_CURRENT_DESKTOP GOOME_BACKEND
export XDG_SESSION_TYPE=x11
pids=()
trap 'kill "${pids[@]}" 2>/dev/null' EXIT
bail() { emit "$cell" 0 fail "$1" "${VER:-}"; exit 0; }
build_lib || bail "lib build failed"
build_prog "$ROOT/examples/hello.c" || bail "hello build failed"

d=":$((RANDOM % 300 + 300))"
Xvfb "$d" -screen 0 1280x720x24 +extension GLX +extension XFIXES +extension SHAPE -nolisten tcp >"$S/xvfb.log" 2>&1 &
pids+=($!)
for i in $(seq 50); do [ -e "/tmp/.X11-unix/X${d#:}" ] && break; sleep 0.1; done
export DISPLAY=$d LIBGL_ALWAYS_SOFTWARE=1
xsetroot -solid '#808080'

wmcmd=()
VER=none
case $wm in
none) ;;
openbox) wmcmd=(openbox) ;;
i3) wmcmd=(i3) ;;
xfwm4) wmcmd=(xfwm4 --compositor=off) ;;
xfwm4-picom) wmcmd=(xfwm4 --compositor=off) ;;
metacity) wmcmd=(metacity --no-composite) ;;
marco) wmcmd=(marco --no-composite) ;;
mutter-x11) wmcmd=(mutter --x11 --sm-disable) ;;
kwin-x11) wmcmd=(kwin_x11) ;;
fluxbox) wmcmd=(fluxbox) ;;
icewm) wmcmd=(icewm) ;;
*) bail "unknown wm" ;;
esac
if [ ${#wmcmd[@]} -gt 0 ]; then
  command -v "${wmcmd[0]}" >/dev/null || bail "${wmcmd[0]} is not installed: $(tailof "$VERIFY_OUT/deps-$wm.log" 6)"
  VER=$("${wmcmd[0]}" --version 2>&1 | head -1)
  mkdir -p "$S/.config/i3"
  printf 'font pango:monospace 8\n' >"$S/.config/i3/config"
  case $wm in
  mutter-x11 | kwin-x11 | marco)
    dbus-run-session -- "${wmcmd[@]}" >"$S/wm.log" 2>&1 &
    ;;
  *)
    "${wmcmd[@]}" >"$S/wm.log" 2>&1 &
    ;;
  esac
  pids+=($!)
  sleep 3
  kill -0 "${pids[-1]}" 2>/dev/null || bail "window manager exited at once: $(tailof "$S/wm.log" 8)"
fi
if [ "$wm" = xfwm4-picom ]; then
  command -v picom >/dev/null || bail "picom missing"
  picom --backend xrender >"$S/picom.log" 2>&1 &
  pids+=($!)
  sleep 2
  VER="$VER + $(picom --version 2>&1 | head -1)"
fi

pix() { import -window root -crop 1x1+"$1"+"$2" txt:- 2>/dev/null | tail -n1 | grep -oE '#[0-9A-Fa-f]{6}' | head -1; }
xev -geometry 300x200+900+450 -event button >"$S/xev.log" 2>&1 &
pids+=($!)
sleep 1
before=$(pix 300 150)

"$VB/hello" >"$S/hello.log" 2>"$S/hello.err" &
pids+=($!)
waitfor "$S/hello.log" '^monitor 0' 60
sleep 1.5
after=$(pix 300 150)
backend=$(sed -n 's/^backend: //p' "$S/hello.log" | head -1)
xdotool mousemove 300 150
sleep 0.3
xdotool click 1
sleep 0.5
hit=no
grep -q '^click 300,150$' "$S/hello.log" && hit=yes
xdotool mousemove 1000 520
sleep 0.3
xdotool click 1
sleep 0.6
through=no
grep -q ButtonPress "$S/xev.log" && through=yes
stolen=no
grep -q '^click 1000,520' "$S/hello.log" && stolen=yes
shown=no
[ -n "$before" ] && [ "$before" != "$after" ] && shown=yes
warn=$(grep -i 'compositor' "$S/hello.err" | head -1)
detail="backend '$backend'; overlay shown (root pixel at 300,150 $before -> $after): $shown; click in hit rect reaches goome: $hit; click outside reaches window below (xev ButtonPress): $through; goome stole outside click: $stolen; goome stderr: $warn; wm: $VER"
if [ "$shown" = yes ] && [ "$hit" = yes ] && [ "$through" = yes ] && [ "$stolen" = no ]; then
  emit "$cell" 1 pass "$detail" "$VER"
else
  emit "$cell" 1 fail "$detail | hello: $(tailof "$S/hello.log" 4) | xev: $(tailof "$S/xev.log" 4) | wm: $(tailof "$S/wm.log" 4)" "$VER"
fi
exit 0
