#!/usr/bin/env bash
# macos ci checks, plain cc and no -framework flags since everything gets dlopened
# the runners cant record the screen, so grabs have to come back as "not allowed" without hanging
set -uo pipefail
cd "$(dirname "$0")/.."
out=build/mac-test
rm -rf "$out"
mkdir -p "$out"
fails=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; fails=$((fails + 1)); }

# runs a command for at most $1 seconds, macos has no timeout(1)
limit() {
  local secs=$1
  shift
  "$@" &
  local pid=$!
  (sleep "$secs"; kill "$pid" 2>/dev/null) &
  local dog=$!
  wait "$pid"
  local rc=$?
  kill "$dog" 2>/dev/null
  wait "$dog" 2>/dev/null
  return $rc
}

if WERROR=1 ./build.sh test >"$out/build.log" 2>&1; then pass "build + unit tests"; else
  cat "$out/build.log"
  fail "build + unit tests"
fi

if otool -L build/hello | grep -qiE 'AppKit|ScreenCaptureKit|CoreGraphics|libobjc'; then
  otool -L build/hello
  fail "hello links no frameworks"
else pass "hello links no frameworks"; fi

python3 tools/single.py "$out/goome.h" >/dev/null
printf '#define GOOME_IMPLEMENTATION\n#include "goome.h"\n' >"$out/impl.c"
if cc -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -I"$out" -o "$out/hello1" "$out/impl.c" examples/hello.c; then
  pass "one file goome.h builds with plain cc"
else fail "one file goome.h builds with plain cc"; fi

# hello has to stay up for 3 s with a real monitor
./build/hello >"$out/hello.log" 2>&1 &
pid=$!
sleep 3
if kill -0 "$pid" 2>/dev/null; then
  alive=1
  kill "$pid"
else alive=0; fi
wait "$pid" 2>/dev/null
cat "$out/hello.log"
if [ "$alive" = 1 ] && grep -q '^backend: macos' "$out/hello.log" && grep -qE '^monitor 0: .* [1-9][0-9]*x[1-9][0-9]* at -?[0-9]+,-?[0-9]+ scale [1-9]' "$out/hello.log"; then
  pass "open: hello runs on macos with a sane monitor"
else fail "open: hello runs on macos with a sane monitor"; fi

# same from the one file build
./"$out/hello1" >"$out/hello1.log" 2>&1 &
pid=$!
sleep 2
if kill -0 "$pid" 2>/dev/null && grep -q '^backend: macos' "$out/hello1.log"; then pass "open: one file hello"; else
  cat "$out/hello1.log"
  fail "open: one file hello"
fi
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

# grabs either get denied clean (normal on ci) or give a real picture if this mac has the permission
check_grab() {
  local name=$1
  shift
  local t0=$SECONDS
  limit 20 ./build/grab "$@" >"$out/$name.log" 2>&1
  local rc=$?
  cat "$out/$name.log"
  local file="${!#}"
  if [ $rc = 0 ] && head -c 2 "$file" | grep -q P6; then
    pass "$name: got a picture ($(head -2 "$file" | tail -1))"
  elif [ $rc = 1 ] && grep -q 'not allowed' "$out/$name.log" && [ $((SECONDS - t0)) -lt 15 ]; then
    pass "$name: no screen recording permission, failed clean"
  else fail "$name (rc $rc)"; fi
}
check_grab "full grab" "$out/full.ppm"
check_grab "region grab" 0 10 10 200 100 "$out/region.ppm"

# test backend still does the same thing here as on linux
if GOOME_BACKEND=test GOOME_TEST_SCRIPT="2 click 100,100; 4 key hi" ./build/hello >"$out/test.log" 2>&1 && grep -q '^backend: test' "$out/test.log"; then
  pass "test backend"
else
  cat "$out/test.log"
  fail "test backend"
fi

echo "$fails failed"
[ "$fails" = 0 ]
