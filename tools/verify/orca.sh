#!/usr/bin/env bash
set -u
source "$(dirname "$0")/lib.sh"
cell="screenreader:linux-orca"
S=$VERIFY_OUT/orca
rm -rf "$S"
mkdir -p "$S/run" "$S/home"
chmod 700 "$S/run"
if [ -z "${ORCA_INNER:-}" ]; then
  export ORCA_INNER=1
  exec env -u DBUS_SESSION_BUS_ADDRESS -u AT_SPI_BUS_ADDRESS XDG_RUNTIME_DIR="$S/run" HOME="$S/home" dbus-run-session -- bash "$0" "$@"
fi
bail() { emit "$cell" 0 fail "$1" "${VER:-}"; exit 0; }
command -v orca >/dev/null || bail "orca not installed: $(tailof "$VERIFY_OUT/deps-orca.log" 6)"
VER=$(orca --version 2>&1 | head -1)
build_lib || bail "lib build failed"
build_prog "$ROOT/tests/a11y_app.c" || bail "a11y_app build failed"
mkdir -p "$HOME/.config/speech-dispatcher"
cat >"$HOME/.config/speech-dispatcher/speechd.conf" <<'EOC'
LogLevel 5
LogDir "default"
DefaultModule dummy
AddModule "dummy" "sd_dummy" "" ""
EOC
mkdir -p "$HOME/.cache/speech-dispatcher/log"
d=":$((RANDOM % 300 + 300))"
Xvfb "$d" -screen 0 1280x800x24 -nolisten tcp >"$S/xvfb.log" 2>&1 &
xv=$!
for i in $(seq 50); do [ -e "/tmp/.X11-unix/X${d#:}" ] && break; sleep 0.1; done
export DISPLAY=$d
unset WAYLAND_DISPLAY
export GSETTINGS_BACKEND=memory
gsettings set org.gnome.desktop.a11y.applications screen-reader-enabled true >"$S/gsettings.log" 2>&1
openbox >"$S/openbox.log" 2>&1 &
ob=$!
gdbus call --session --dest org.a11y.Bus --object-path /org/a11y/bus --method org.a11y.Bus.GetAddress >"$S/bus.txt" 2>&1
orca --replace --debug --debug-file="$S/orca.debug" >"$S/orca.out" 2>&1 &
orcapid=$!
sleep 6
kill -0 $orcapid 2>/dev/null || echo "orca exited early" >>"$S/orca.out"
GOOME_BACKEND=x11 GTK_A11Y=none NO_AT_BRIDGE=1 "$VB/a11y_app" 60 >"$S/app.log" 2>&1 &
app=$!
sleep 3
xdotool search --name '^goome a11y$' windowfocus 2>/dev/null
xdotool mousemove 200 200 click 1
sleep 1
cat >"$S/drive.py" <<'EOP'
import sys, time
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, GLib
def find(o, role, name):
    if o.get_role_name() == role and o.get_name() == name:
        return o
    for i in range(o.get_child_count()):
        c = o.get_child_at_index(i)
        r = c and find(c, role, name)
        if r:
            return r
desk = Atspi.get_desktop(0)
app = None
for i in range(desk.get_child_count()):
    a = desk.get_child_at_index(i)
    if a and a.get_name() == "goome a11y":
        app = a
if not app:
    print("no app on bus")
    sys.exit(1)
loop = GLib.MainLoop()
for role, name in (("entry", "Name"), ("push button", "Press me"), ("check box", "Dark mode")):
    o = find(app, role, name)
    print("focus", name, bool(o) and Atspi.Component.grab_focus(o))
    GLib.timeout_add(1500, loop.quit)
    loop.run()
EOP
python3 -I "$S/drive.py" >"$S/drive.log" 2>&1
xdotool key Tab
sleep 1
xdotool key Tab
sleep 2
kill $app 2>/dev/null
sleep 1
cp "$HOME"/.cache/speech-dispatcher/log/*.log "$S/" 2>/dev/null
speech=$(grep -iE 'SPEECH OUTPUT|speak' "$S/orca.debug" 2>/dev/null | grep -iE "press me|dark mode|name" | head -6 | tr '\n' ';' | cut -c1-900)
nspeech=$(grep -ciE 'SPEECH OUTPUT' "$S/orca.debug" 2>/dev/null)
if grep -iE 'SPEECH OUTPUT' "$S/orca.debug" 2>/dev/null | grep -qi 'press me'; then
  emit "$cell" 1 pass "orca $VER spoke the button label. debug lines: $speech. total SPEECH OUTPUT lines $nspeech" "$VER"
else
  emit "$cell" 1 fail "orca $VER did not speak the button label. SPEECH OUTPUT lines $nspeech: $(grep -iE 'SPEECH OUTPUT' "$S/orca.debug" 2>/dev/null | head -5 | tr '\n' ';' | cut -c1-500) | drive: $(tailof "$S/drive.log" 5) | orca.out: $(tailof "$S/orca.out" 6) | debug tail: $(tailof "$S/orca.debug" 6)" "$VER"
fi
kill $orcapid $ob $xv 2>/dev/null
exit 0
