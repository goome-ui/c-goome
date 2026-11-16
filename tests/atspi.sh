#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
out="${ATSPI_SCRATCH:-$PWD/build/atspi}"
mkdir -p "$out"
if [ -z "${ATSPI_INNER:-}" ]; then
  export ATSPI_INNER=1
  rt="$out/run"
  mkdir -p "$rt" && chmod 700 "$rt"
  exec env -u DBUS_SESSION_BUS_ADDRESS -u AT_SPI_BUS_ADDRESS XDG_RUNTIME_DIR="$rt" dbus-run-session -- bash "$0" "$@"
fi
cc="${CC:-cc}"
"$cc" -std=c99 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Iinclude -o "$out/a11y_app" tests/a11y_app.c build/libgoome.a -ldl || exit 1
d=":$((RANDOM % 400 + 300))"
Xvfb "$d" -screen 0 1024x768x24 -nolisten tcp >"$out/xvfb.log" 2>&1 &
xv=$!
for i in $(seq 50); do [ -e "/tmp/.X11-unix/X${d#:}" ] && break; sleep 0.1; done
export DISPLAY="$d" GOOME_BACKEND=x11 GTK_A11Y=none NO_AT_BRIDGE=1
gdbus call --session --dest org.a11y.Bus --object-path /org/a11y/bus --method org.a11y.Bus.GetAddress >"$out/bus.txt" 2>&1
echo "a11y bus: $(cat "$out/bus.txt")"
reg=/usr/libexec/at-spi2-registryd
[ -x "$reg" ] || reg=/usr/lib/at-spi2-core/at-spi2-registryd
"$out/a11y_app" 40 >"$out/app.log" 2>&1 &
app=$!
sleep 1
python3 tests/atspi_check.py "goome a11y" | tee "$out/check.log"
rc=${PIPESTATUS[0]}
for i in $(seq 30); do kill -0 $app 2>/dev/null || break; sleep 0.2; done
kill $app 2>/dev/null
wait $app 2>/dev/null
echo "--- app said:"
cat "$out/app.log"
grep -q "pressed 1" "$out/app.log" || { echo "FAIL the app never saw the press"; rc=1; }
grep -q "dark 1" "$out/app.log" || { echo "FAIL the app never saw the check box toggle"; rc=1; }
grep -q "volume 7.5" "$out/app.log" || { echo "FAIL the app never saw the slider value"; rc=1; }
grep -q "name Robin" "$out/app.log" || { echo "FAIL the app never saw the new name"; rc=1; }
grep -q "bye fruit 2" "$out/app.log" || { echo "FAIL the combo pick never landed"; rc=1; }
kill $xv 2>/dev/null
[ $rc = 0 ] && echo "atspi ok" || echo "atspi FAILED"
exit $rc
