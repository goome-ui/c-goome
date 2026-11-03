#!/usr/bin/env bash
# wayland backend tests on headless sway (pixman), never touches the real session
# tests/wayland.sh [SAN=1 in env for asan+ubsan], takes ~2 min
# needs sway swaybg swaymsg grim Xwayland xev wtype python3, and wayland-client.h for the vptr helper (WL_INCLUDE)
set -u
cd "$(dirname "$0")/.."
S=${SCRATCH:-$PWD/test-out/wayland}
INC=${WL_INCLUDE:-/usr/include}
WTYPE=${WTYPE:-$(command -v wtype || echo "$S/root/usr/bin/wtype")}
BIN=$S/bin${SAN:+-san}
RT=$S/rt
IMG="python3 tests/wl_img.py"
sv=$(sway --version | grep -oE "[0-9]+\.[0-9]+" | head -1)
OLD_SWAY=$([ "${sv%%.*}" = 1 ] && [ "${sv#*.}" -lt 10 ] && echo 1 || echo 0)
mkdir -p "$S/tmp" "$S/out"
export TMPDIR=$S/tmp XDG_RUNTIME_DIR=$RT WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE GOOME_BACKEND
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1
O=$S/out
npass=0 nfail=0
pass() { echo "PASS $*"; npass=$((npass + 1)); }
fail() { echo "FAIL $*"; nfail=$((nfail + 1)); }
check() { local name=$1; shift; if "$@"; then pass "$name"; else fail "$name"; fi; }

build() {
  mkdir -p "$BIN/obj"
  local fl=(-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Iinclude -Isrc -g -O1)
  [ -n "${SAN:-}" ] && fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  local objs=()
  for f in src/*.c; do cc "${fl[@]}" -c "$f" -o "$BIN/obj/$(basename "${f%.c}").o" || return 1; objs+=("$BIN/obj/$(basename "${f%.c}").o"); done
  rm -f "$BIN/libgoome.a"; ar rcs "$BIN/libgoome.a" "${objs[@]}"
  for f in examples/hello.c examples/grab.c tests/wl_watch.c; do cc "${fl[@]}" -o "$BIN/$(basename "${f%.c}")" "$f" "$BIN/libgoome.a" || return 1; done
  cc -O1 -I"$INC" -o "$S/wl_vptr" tests/wl_vptr.c -l:libwayland-client.so.0
}

SWAYPID=
start_sway() { # $1 = name, rest = sway config lines
  stop_sway
  T=$1; shift
  rm -rf "$RT"; mkdir -p "$RT"; chmod 700 "$RT"
  { printf '%s\n' "$@"; echo "exec swaybg -o '*' -i $S/bg.png -m stretch"; echo "default_border none"; echo "xwayland enable"; } > "$S/sway-$T.cfg"
  sway -c "$S/sway-$T.cfg" > "$O/sway-$T.log" 2>&1 &
  SWAYPID=$!
  local sock=
  for _ in $(seq 60); do
    sock=$(ls "$RT" 2>/dev/null | grep -E '^wayland-[0-9]+$' | head -1)
    [ -n "$sock" ] && ls "$RT"/sway-ipc.* >/dev/null 2>&1 && break
    sleep 0.1
  done
  export WAYLAND_DISPLAY=$sock SWAYSOCK=$(ls "$RT"/sway-ipc.* | head -1)
  sleep 2 # swaybg
}
stop_sway() {
  [ -n "$SWAYPID" ] && kill "$SWAYPID" 2>/dev/null && wait "$SWAYPID" 2>/dev/null
  SWAYPID=
  pkill -f "xev -event button" 2>/dev/null
  return 0
}
trap stop_sway EXIT

san_clean() { # no sanitizer reports in any of these logs
  ! grep -qE 'ERROR: (Address|Leak)Sanitizer|runtime error:' "$@"
}
waitfor() { # file pattern [tenths]
  for _ in $(seq "${3:-30}"); do grep -qE "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
  return 1
}
pxis() { # file x y "r g b" [tolerance]
  local got; got=$($IMG px "$1" "$2" "$3")
  python3 -c "import sys; a=list(map(int,'$got'.split())); b=list(map(int,'$4'.split())); sys.exit(0 if max(abs(x-y) for x,y in zip(a,b)) <= ${5:-0} else 1)" || { echo "  pixel $2,$3 is $got, wanted $4"; return 1; }
}
same() { # a b [region...], max diff <= 2 and nothing past it
  local r; r=$($IMG cmp "$@")
  [ "$r" = "0 0" ] || { local w=${r%% *}; [ "${r#* }" = "0" ] && [ "$w" -le 2 ] 2>/dev/null; } || { echo "  cmp $* -> $r"; return 1; }
}
exact() { local r; r=$($IMG cmp "$@"); [ "$r" = "0 0" ] || { echo "  cmp $* -> $r"; return 1; }; }

# what hello should look like on top of bg, from the test backend: $1 bg ppm, $2 scale, $3 out ppm
test_shot() {
  local d=$O/shots-$T; rm -rf "$d"; mkdir -p "$d"
  local wh; wh=$($IMG size "$1")
  # slow shots keep hello from ever polling, so it wont see the end. stop it once frame 1 is out
  ( ulimit -f 200000; GOOME_TEST_FRAMES=$1 GOOME_TEST_SCRIPT="screen ${wh% *}x${wh#* } $2; end 3" GOOME_TEST_SHOTS=$d exec "$BIN/hello" > "$O/shot-$T.log" 2>&1 ) &
  local sp=$!
  for _ in $(seq 100); do [ -f "$d/000002.ppm" ] && break; kill -0 $sp 2>/dev/null || break; sleep 0.05; done
  kill $sp 2>/dev/null; wait $sp 2>/dev/null
  cp "$d/000001.ppm" "$3"
  rm -rf "$d"
}

# hello on one output: open, draw (grim + our grab vs the test backend), click-through, keys, capture
hello_run() { # name scale "sway output line" lw lh
  local name=$1 sc=$2 line=$3 lw=$4 lh=$5
  start_sway "$name" "$line"
  grim -t ppm "$O/bg-$T.ppm"
  "$BIN/hello" > "$O/hello-$T.log" 2>&1 &
  local hp=$!
  sleep 1.5
  check "$name open" grep -qE "^backend: wayland \+ wlr" "$O/hello-$T.log"
  check "$name monitor" grep -qE "^monitor 0: HEADLESS-1 ${lw}x${lh} at 0,0 scale $sc\.00" "$O/hello-$T.log"
  grim -t ppm "$O/full-$T.ppm"
  test_shot "$O/bg-$T.ppm" "$sc" "$O/want-$T.ppm"
  local bx=$((80 * sc)) bw=$((280 * sc)) bh=$((120 * sc))
  if [ "$OLD_SWAY" = 1 ] && [ "${name#*flip}" != "$name" ]; then
    echo "info $name draw grim skipped, sway < 1.10 grabs flipped outputs mirrored (grim too)"
  else
    check "$name draw grim" same "$O/full-$T.ppm" "$O/want-$T.ppm" "$bx" "$bx" "$bw" "$bh" "$bx" "$bx"
  fi
  "$BIN/grab" 0 "$O/grab-$T.ppm" > "$O/grab-$T.log" 2>&1
  check "$name draw grab full" exact "$O/grab-$T.ppm" "$O/full-$T.ppm"
  check "$name grab says self" grep -q "has our overlay" "$O/grab-$T.log"
  check "$name grab no cursor" bash -c "! grep -q 'has the cursor' '$O/grab-$T.log'"
  "$BIN/grab" 0 60 40 300 200 "$O/rgrab-$T.ppm" > "$O/rgrab-$T.log" 2>&1
  grim -t ppm -g "60,40 300x200" "$O/rgrim-$T.ppm"
  check "$name capture region vs grim" exact "$O/rgrab-$T.ppm" "$O/rgrim-$T.ppm"
  "$S/wl_vptr" "$lw" "$lh" m 600 520 > /dev/null
  # cursor sits at 600,520 now, a grab there has to be plain background
  "$BIN/grab" 0 560 480 100 80 "$O/cgrab-$T.ppm" > /dev/null 2>&1
  check "$name capture without cursor" same "$O/cgrab-$T.ppm" "$O/bg-$T.ppm" 0 0 $((100 * sc)) $((80 * sc)) $((560 * sc)) $((480 * sc))
  if [ "$name" = scale1 ]; then
    check "$name capture is the known picture" exact "$O/cgrab-$T.ppm" "$S/bg.ppm" 0 0 100 80 560 480
  fi
  swaymsg -q exec "xev -event button > $O/xev-$T.log 2>&1" # a window under us that logs clicks
  for _ in $(seq 50); do swaymsg -t get_tree | grep -qi '"class": "xev"' && break; sleep 0.1; done
  sleep 0.5
  "$S/wl_vptr" "$lw" "$lh" c 220 150 c 600 500 > /dev/null
  sleep 0.5
  check "$name click on box" grep -q "^click 220,150" "$O/hello-$T.log"
  check "$name click through" waitfor "$O/xev-$T.log" ButtonPress 20
  check "$name click next to box not ours" bash -c "! grep -q '^click 600,500' '$O/hello-$T.log'"
  "$S/wl_vptr" "$lw" "$lh" c 220 174 > /dev/null # the type bar
  sleep 0.4
  "$WTYPE" -s 400 'Hi é!' -k Left -k F5 -M ctrl a -m ctrl -k BackSpace -s 200 -k Escape
  sleep 0.8
  check "$name key shift letter" grep -q "^key 104 mods 100 text 'H'" "$O/hello-$T.log"
  check "$name key letter" grep -q "^key 105 mods 0 text 'i'" "$O/hello-$T.log"
  check "$name key non ascii text" grep -q "^key 0 mods 0 text 'é'" "$O/hello-$T.log"
  check "$name key shifted symbol" grep -q "^key 49 mods 100 text '!'" "$O/hello-$T.log"
  check "$name key arrow" grep -q "^key 144 mods 0 text ''" "$O/hello-$T.log"
  check "$name key f5" grep -q "^key 133 mods 0 text ''" "$O/hello-$T.log"
  check "$name key ctrl+a" grep -q "^key 97 mods 200 text ''" "$O/hello-$T.log"
  check "$name key backspace" grep -q "^key 8 mods 0 text ''" "$O/hello-$T.log"
  for _ in $(seq 20); do kill -0 $hp 2>/dev/null || break; sleep 0.1; done
  if kill -0 $hp 2>/dev/null; then fail "$name esc quits"; kill $hp; else wait $hp; check "$name esc quits clean" test $? = 0; fi
  check "$name sanitizers" san_clean "$O/hello-$T.log"
  check "$name sanitizers grab" san_clean "$O/grab-$T.log"
}

# a box on every output, hotplug, mixed scale, scroll, drag, empty canvas
watch_run() {
  start_sway multi "output HEADLESS-1 resolution 1280x720 position 0 0"
  local st=$S/st; rm -rf "$st"
  (cd "$O" && exec "$BIN/wl_watch" "$st" > "$O/watch.log" 2>&1) & # control lines are 63 chars, so grabs use short names in $O
  local wp=$!
  waitfor "$O/watch.log" "^mon 0" 30
  check "multi open one" grep -q "^monitors 1" "$O/watch.log"
  check "no global pointer off hyprland" bash -c "! grep -q 'global pointer' '$O/watch.log'"
  ctl() { echo "$1" > "$st/control"; sleep "${2:-0.4}"; }
  "$S/wl_vptr" 1280 720 m 220 150 s 2 s -1 a 25 d m 700 400 u > /dev/null
  sleep 0.4
  check "scroll wheel notches" grep -q "^scroll 2 220,150" "$O/watch.log"
  check "scroll wheel up" grep -q "^scroll -1 220,150" "$O/watch.log"
  check "scroll touchpad adds up" grep -q "^scroll 2 220,150" <(grep "^scroll" "$O/watch.log" | tail -1)
  check "drag down" grep -q "^down 1 220,150 mon 0" "$O/watch.log"
  check "drag held" grep -q "^held 1 at" "$O/watch.log"
  check "drag motion off the box" grep -q "^motion 700,400" "$O/watch.log"
  check "drag up off the box" grep -q "^up 1 700,400" "$O/watch.log"

  swaymsg -q create_output
  sleep 0.3
  swaymsg -q output HEADLESS-2 resolution 2048x1536 position 1280 0 scale 2
  sleep 1
  check "hotplug add" grep -q "^mon 1 id [0-9]* HEADLESS-2 1024x768+1280+0 scale 2.00 px 2048x1536 rot 0" "$O/watch.log"
  grim -t ppm -o HEADLESS-2 "$O/out2.ppm"
  check "mixed scale box real px" pxis "$O/out2.ppm" 440 300 "51 102 204"
  check "mixed scale box edge" bash -c "! $IMG px '$O/out2.ppm' 159 300 | grep -qx '51 102 204'"
  check "mixed scale box edge in" pxis "$O/out2.ppm" 160 300 "51 102 204"
  "$S/wl_vptr" 2304 768 c 1500 150 > /dev/null
  sleep 0.3
  check "click second output" grep -q "^down 1 1500,150 mon 1" "$O/watch.log"
  ctl "grab 1 w-full1.ppm" 0.8
  check "grab second output full" exact "$O/w-full1.ppm" "$O/out2.ppm"
  ctl "grab 1 1300 20 400 300 w-reg1.ppm" 0.8
  grim -t ppm -g "1300,20 400x300" "$O/g-reg1.ppm"
  check "grab second output region" exact "$O/w-reg1.ppm" "$O/g-reg1.ppm"

  ctl empty 0.6
  grim -t ppm -o HEADLESS-1 "$O/empty.ppm"
  check "empty canvas shows nothing" exact "$O/empty.ppm" "$S/bg.ppm" 80 80 280 120 80 80
  local downs; downs=$(grep -c "^down" "$O/watch.log")
  "$S/wl_vptr" 2304 768 c 220 150 > /dev/null
  sleep 0.3
  check "empty canvas takes no clicks" test "$(grep -c "^down" "$O/watch.log")" = "$downs"
  ctl draw 0.8
  grim -t ppm -o HEADLESS-1 "$O/redraw.ppm"
  check "draws again after empty" pxis "$O/redraw.ppm" 220 150 "51 102 204"
  "$S/wl_vptr" 2304 768 c 220 150 > /dev/null
  sleep 0.3
  check "clicks again after empty" test "$(grep -c "^down" "$O/watch.log")" -gt "$downs"

  swaymsg -q output HEADLESS-2 unplug
  sleep 0.8
  check "hotplug remove" grep -q "^monitors 1" <(grep "^monitors" "$O/watch.log" | tail -1)
  swaymsg -q create_output
  sleep 1
  check "hotplug add again" grep -q "^mon 1 id [0-9]* HEADLESS-3 " "$O/watch.log"
  ctl "grab 1 w-full3.ppm" 0.8
  check "grab replugged output" grep -q "^grabbed 0: " <(grep "^grabbed" "$O/watch.log" | tail -1)
  swaymsg -q output HEADLESS-3 unplug
  sleep 0.8
  check "hotplug remove again" grep -q "^monitors 1" <(grep "^monitors" "$O/watch.log" | tail -1)
  ctl "grab 1 w-gone.ppm" 0.5
  check "grab gone monitor fails clean" grep -q "^grabbed -4" <(grep "^grabbed" "$O/watch.log" | tail -1)
  ctl "grab 0 w-full0.ppm" 0.8
  grim -t ppm -o HEADLESS-1 "$O/g-full0.ppm"
  check "grab first output after hotplug" exact "$O/w-full0.ppm" "$O/g-full0.ppm"
  ctl quit 0.5
  for _ in $(seq 20); do kill -0 $wp 2>/dev/null || break; sleep 0.1; done
  if kill -0 $wp 2>/dev/null; then fail "watch quits"; kill $wp; else wait $wp; check "watch quits clean" test $? = 0; fi
  check "watch sanitizers" san_clean "$O/watch.log"
}

# hyprland: no real one here, a fake ipc socket that answers cursorpos like hyprctl does
hypr_run() {
  start_sway hypr "output HEADLESS-1 resolution 1280x720 position 0 0"
  mkdir -p "$RT/hypr/testsig"
  python3 -c '
import os, socket, sys
s = socket.socket(socket.AF_UNIX); s.bind(sys.argv[1]); s.listen(5)
while True:
    c, _ = s.accept()
    if c.recv(64) == b"cursorpos": c.sendall(b"800, 255")
    c.close()' "$RT/hypr/testsig/.socket.sock" &
  local fp=$! st=$S/st-hypr; rm -rf "$st"
  sleep 0.3
  (cd "$O" && HYPRLAND_INSTANCE_SIGNATURE=testsig exec "$BIN/wl_watch" "$st" > "$O/hypr.log" 2>&1) &
  local wp=$!
  waitfor "$O/hypr.log" "^mon 0" 30
  check "hypr global pointer cap" grep -q "global pointer$" "$O/hypr.log"
  echo "grab 0 700 200 300 200 h-c.ppm" > "$st/control"; sleep 0.6
  echo "grab 0 0 400 300 200 h-n.ppm" > "$st/control"; sleep 0.6
  check "hypr cursor in grab" grep -q "^grabbed 0: 300x200 of 700,200 300x200 self 1 cursor 1" "$O/hypr.log"
  check "hypr cursor not in other grab" grep -q "^grabbed 0: 300x200 of 0,400 300x200 self 1 cursor 0" "$O/hypr.log"
  echo quit > "$st/control"; sleep 0.5
  kill $wp $fp 2>/dev/null; wait $wp $fp 2>/dev/null
  check "hypr sanitizers" san_clean "$O/hypr.log"
}

opens_clean() {
  local r
  WAYLAND_DISPLAY= GOOME_BACKEND=wayland "$BIN/hello" > "$O/nodpy.log" 2>&1; r=$?
  check "no WAYLAND_DISPLAY fails clean" test $r = 1
  WAYLAND_DISPLAY=nothing-here GOOME_BACKEND=wayland "$BIN/hello" > "$O/baddpy.log" 2>&1; r=$?
  check "dead WAYLAND_DISPLAY fails clean" test $r = 1
  check "fail logs sanitizer clean" san_clean "$O/nodpy.log" "$O/baddpy.log"
}

build || { echo "FAIL build"; exit 1; }
[ -f "$S/bg.png" ] || $IMG bg 1280 720 "$S/bg.png" "$S/bg.ppm"
opens_clean
hello_run scale1 1 "output HEADLESS-1 resolution 1280x720 position 0 0" 1280 720
hello_run scale2 2 "output HEADLESS-1 resolution 2560x1440 position 0 0 scale 2" 1280 720
hello_run rot90 1 "output HEADLESS-1 resolution 1280x720 position 0 0 transform 90" 720 1280
hello_run rot270flip-scale2 2 "output HEADLESS-1 resolution 2560x1440 position 0 0 scale 2 transform flipped-270" 720 1280
watch_run
hypr_run
stop_sway
[ "$nfail" = 0 ] && rm -f "$O"/*.ppm
echo "$npass passed, $nfail failed"
[ "$nfail" = 0 ]
