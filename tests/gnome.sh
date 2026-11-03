#!/usr/bin/env bash
# capture only test of the mutter backend on the live gnome session, nothing pops up or gets drawn
# then the portal code against tests/fakeportal.py on a private bus (never the real portal, that one pops a dialog)
# tests/gnome.sh [outdir]   (default test-out/gnome)
set -uo pipefail
cd "$(dirname "$0")/.."
out="${1:-test-out/gnome}"
mkdir -p "$out"
export TMPDIR="$out"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$(id -u)/bus}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
rc=0
bin="${GN_BIN:-$out/gnome_cap}" # GN_BIN = a sanitizer build of tests/gnome_cap.c

WERROR=1 ./build.sh >/dev/null || exit 1
cc -O1 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -Iinclude -Isrc \
  -o "$out/gnome_cap" tests/gnome_cap.c build/libgoome.a || exit 1

# logical monitors from mutter, the same areas the x11 overlay sees without xwayland scaling
mons=$(busctl --user --json=short call org.gnome.Mutter.DisplayConfig /org/gnome/Mutter/DisplayConfig \
  org.gnome.Mutter.DisplayConfig GetCurrentState | python3 -I -c '
import json, sys
serial, monitors, logical, props = json.load(sys.stdin)["data"]
logical_mode = props.get("layout-mode", {}).get("data", 1) == 1
size = {}
for spec, modes, mprops in monitors:
    for m in modes:
        if m[6].get("is-current", {}).get("data"):
            size[spec[0]] = (m[1], m[2])
for x, y, scale, transform, primary, specs, lprops in logical:
    conn = specs[0][0]
    if conn not in size:
        continue
    w, h = size[conn]
    if transform in (1, 3, 5, 7):
        w, h = h, w
    if logical_mode:
        w, h = round(w / scale), round(h / scale)
    print("%s:%d,%d,%d,%d,%d" % (conn, x, y, w, h, round(scale)))
' | tr '\n' ' ')
echo "monitors: $mons"
[ -n "$mons" ] || { echo "FAIL: mutter gave no monitors"; exit 1; }

echo "== mutter, live session"
GOOME_GNOME_RESTART_AT=60 timeout 60 "$bin" "$out" $mons || rc=1

fake="$out/fake"
rm -rf "$fake"
mkdir -p "$fake/state"
cat > "$fake/bus.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><listen>unix:path=$fake/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
EOF
dbus-daemon --config-file="$fake/bus.conf" --nofork --nopidfile &
busd=$!
fpid=
cleanup() {
  [ -n "$fpid" ] && kill "$fpid" 2>/dev/null
  kill "$busd" 2>/dev/null
  true
}
trap cleanup EXIT
for _ in $(seq 50); do [ -S "$fake/bus" ] && break; sleep 0.1; done
real_bus=$DBUS_SESSION_BUS_ADDRESS
fakebus="unix:path=$fake/bus"

portal_run() { # name, then env for the fake
  local name=$1
  shift
  : > "$fake/$name.log"
  env "$@" FAKE_LOG="$fake/$name.log" REAL_BUS="$real_bus" REAL_RUNTIME="$XDG_RUNTIME_DIR" \
    DBUS_SESSION_BUS_ADDRESS="$fakebus" python3 -I tests/fakeportal.py > "$fake/$name.out" 2>&1 &
  fpid=$!
  for _ in $(seq 50); do grep -q ready "$fake/$name.out" 2>/dev/null && break; sleep 0.1; done
}
portal_stop() {
  kill "$fpid" 2>/dev/null
  wait "$fpid" 2>/dev/null
  fpid=
}
ok() { echo "pass: $1"; }
bad() { echo "FAIL: $1"; rc=1; }
calls() { grep "\"call\": \"$2\"" "$fake/$1.log"; }

echo "== portal, user says no"
portal_run deny FAKE_MODE=deny
DBUS_SESSION_BUS_ADDRESS="$fakebus" GN_CAP=portal GN_EXPECT=denied GN_STATE="$fake/state" \
  timeout 60 "$bin" "$out" $mons || rc=1
portal_stop

echo "== portal, user says yes, restart by the test hook"
portal_run ok FAKE_MODE=ok
DBUS_SESSION_BUS_ADDRESS="$fakebus" GN_CAP=portal GN_STATE="$fake/state" GOOME_GNOME_RESTART_AT=60 \
  timeout 60 "$bin" "$out" $mons || rc=1
portal_stop
sel=$(calls ok SelectSources | head -1)
if grep -q '"types": 1' <<<"$sel" && grep -q '"multiple": true' <<<"$sel" && grep -q '"cursor_mode": 1' <<<"$sel" &&
  grep -q '"persist_mode": 2' <<<"$sel"; then
  ok "SelectSources asks for monitors, multiple, hidden cursor, persist_mode 2"
else bad "SelectSources options: $sel"; fi
if grep -q restore_token <<<"$sel"; then bad "first SelectSources had a token with no token saved"; else ok "first SelectSources has no token"; fi
if [ "$(cat "$fake/state/portal-token" 2>/dev/null)" = 0f1e2d3c-aaaa-4bbb-8ccc-123456789abc ]; then
  ok "restore token saved to state_dir/portal-token"
else bad "no restore token in $fake/state/portal-token"; fi
if calls ok Close >/dev/null && calls ok SelectSources | sed -n 2p | grep -q 0f1e2d3c; then
  ok "restart closes the old session and passes the token on"
else bad "restart didnt close the old session and reuse the token"; fi

echo "== portal, token from disk, portal sends Session.Closed"
portal_run closed FAKE_MODE=ok FAKE_CLOSE_AFTER=1
DBUS_SESSION_BUS_ADDRESS="$fakebus" GN_CAP=portal GN_STATE="$fake/state" \
  timeout 60 "$bin" "$out" $mons || rc=1
portal_stop
if calls closed SelectSources | head -1 | grep -q 0f1e2d3c; then ok "token from disk goes into the first SelectSources"
else bad "token from disk not used"; fi
if grep -A20 '"EmitClosed"' "$fake/closed.log" | grep -q '"call": "CreateSession"'; then ok "Session.Closed starts a new session"
else bad "no new session after Session.Closed"; fi

echo "== $([ $rc = 0 ] && echo all passed || echo SOME FAILED)"
exit $rc
