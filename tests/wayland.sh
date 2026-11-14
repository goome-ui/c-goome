#!/usr/bin/env bash
# wayland backend tests on headless sway (pixman), never touches the real session
# tests/wayland.sh [SAN=1 in env for asan+ubsan], takes ~3 min, WL_ONLY=win for just the window mode part
# needs sway swaybg swaymsg grim Xwayland xev wtype python3, and wayland-client.h for the vptr helper (WL_INCLUDE)
# window mode also wants wl-copy/wl-paste, and weston + weston-screenshooter (WESTON, WESTON_SHOT) for the client side title bar
set -u
cd "$(dirname "$0")/.."
S=${SCRATCH:-$PWD/test-out/wayland}
INC=${WL_INCLUDE:-/usr/include}
WTYPE=${WTYPE:-$(command -v wtype || echo "$S/root/usr/bin/wtype")}
WLCOPY=${WLCOPY:-$(command -v wl-copy || echo "$S/root/usr/bin/wl-copy")}
WLPASTE=${WLPASTE:-$(command -v wl-paste || echo "$S/root/usr/bin/wl-paste")}
BIN=$S/bin${SAN:+-san}
RT=$S/rt
IMG="python3 tests/wl_img.py"
sv=$(sway --version | grep -oE "[0-9]+\.[0-9]+" | head -1)
OLD_SWAY=$([ "${sv%%.*}" = 1 ] && [ "${sv#*.}" -lt 10 ] && echo 1 || echo 0)
mkdir -p "$S/tmp" "$S/out"
export TMPDIR=$S/tmp XDG_RUNTIME_DIR=$RT WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE GOOME_BACKEND
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1
printf 'leak:libfontconfig.so\n' > "$S/lsan.supp"
export LSAN_OPTIONS=suppressions=$S/lsan.supp
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
  for f in examples/hello.c examples/grab.c examples/app.c tests/wl_watch.c tests/win_two.c; do cc "${fl[@]}" -o "$BIN/$(basename "${f%.c}")" "$f" "$BIN/libgoome.a" || return 1; done
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

rect() { # x y w h of the app window in sway
  swaymsg -t get_tree | python3 -c '
import json, sys
def walk(n):
    if n.get("app_id") == "goome app": return n
    for c in n.get("nodes", []) + n.get("floating_nodes", []):
        r = walk(c)
        if r: return r
n = walk(json.load(sys.stdin))
r = n["rect"] if n else {"x": -1, "y": -1, "width": 0, "height": 0}
print(r["x"], r["y"], r["width"], r["height"])'
}
quits() { # pid name
  for _ in $(seq 30); do kill -0 "$1" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$1" 2>/dev/null; then fail "$2 app exits"; kill "$1"; else wait "$1"; check "$2 app exits clean" test $? = 0; fi
}

# window mode on sway: server side decorations, then forced client side ones (GOOME_CSD) so the title bar gets clicked too
win_run() {
  start_sway win "output HEADLESS-1 resolution 1280x720 position 0 0" 'for_window [app_id="goome app"] floating enable'
  local L=$O/app-win.log
  GOOME_BACKEND=wayland "$BIN/app" > "$L" 2>&1 &
  local ap=$!
  waitfor "$L" "^window " 30
  sleep 0.6
  check "win open caps (keyboard window clipboard cursor)" grep -q "^backend wayland caps f0$" "$L"
  check "win open size" grep -q "^window 640x420 scale 1.00 px 640x420$" "$L"
  read -r x y w h <<<"$(rect)"
  check "win sway sees 640x420 ($w x $h)" test "$w $h" = "640 420"
  check "win gets focus" waitfor "$L" "^focus in$" 20
  grim -t ppm "$O/win-open.ppm"
  check "win draw top bar" pxis "$O/win-open.ppm" $((x + 300)) $((y + 30)) "38 42 52"
  check "win draw body" pxis "$O/win-open.ppm" $((x + 300)) $((y + 400)) "24 26 32"
  check "win draw right edge" pxis "$O/win-open.ppm" $((x + 639)) $((y + 200)) "58 98 200"
  swaymsg -q '[app_id="goome app"] resize set 800 500'
  check "win resize sends monitors" waitfor "$L" "^resized 800x500 scale 1.00 px 800x500$" 30
  sleep 0.4
  read -r x y w h <<<"$(rect)"
  grim -t ppm "$O/win-resized.ppm"
  check "win resize canvas follows" pxis "$O/win-resized.ppm" $((x + 799)) $((y + 300)) "58 98 200"
  check "win resize bottom edge" pxis "$O/win-resized.ppm" $((x + 400)) $((y + 499)) "58 98 200"
  "$S/wl_vptr" 1280 720 c $((x + 420)) $((y + 400)) > /dev/null
  check "win click in window coords" waitfor "$L" "^down 420,400 button 1$"
  "$S/wl_vptr" 1280 720 c $((x + 60)) $((y + 210)) > /dev/null
  check "win copy" waitfor "$L" "^copy 'copied from goome 1'$"
  check "win copy seen by wl-paste" test "$(timeout 3 "$WLPASTE" -n)" = "copied from goome 1"
  printf 'h\xc3\xa9llo from wl-copy' | "$WLCOPY"
  sleep 0.3
  "$S/wl_vptr" 1280 720 c $((x + 180)) $((y + 210)) > /dev/null
  check "win paste from wl-copy, utf-8 kept" waitfor "$L" "^paste 'héllo from wl-copy'$"
  head -c 400000 /dev/zero | tr '\0' q | "$WLCOPY"
  sleep 0.3
  "$WTYPE" -s 300 -M ctrl v -m ctrl
  check "win big paste gets every byte" waitfor "$L" "^pasted 400000 bytes$" 30
  "$WTYPE" -s 300 -M ctrl c -m ctrl
  check "win ctrl+c" waitfor "$L" "^copy 'copied from goome 2'$"
  "$WTYPE" -s 300 -M ctrl v -m ctrl
  check "win paste own copy" waitfor "$L" "^paste 'copied from goome 2'$"
  "$S/wl_vptr" 1280 720 m $((x + 60)) $((y + 276)) > /dev/null
  check "win cursor ibeam" waitfor "$L" "^cursor 1$"
  "$S/wl_vptr" 1280 720 m $((x + 300)) $((y + 400)) > /dev/null
  check "win cursor back to arrow" waitfor "$L" "^cursor 0$"
  "$S/wl_vptr" 1280 720 m $((x + 200)) $((y + 15)) d m $((x + 300)) $((y + 75)) w 100 m $((x + 320)) $((y + 95)) w 100 u > /dev/null
  sleep 0.4
  read -r nx ny _ _ <<<"$(rect)"
  check "win drag top bar moves it ($x,$y -> $nx,$ny)" test "$nx $ny" = "$((x + 120)) $((y + 80))"
  x=$nx y=$ny
  swaymsg -q output HEADLESS-1 scale 2
  check "win scale 2" waitfor "$L" "^resized 800x500 scale 2.00 px 1600x1000$" 30
  swaymsg -q output HEADLESS-1 scale 1.5
  check "win scale 1.5 (fractional)" waitfor "$L" "^resized 800x500 scale 1.50 px 1200x750$" 30
  sleep 0.5
  grim -t ppm "$O/win-15.ppm"
  swaymsg -q output HEADLESS-1 scale 1
  check "win scale back to 1" waitfor "$L" "^resized 800x500 scale 1.00 px 800x500$" 30
  "$S/wl_vptr" 1280 720 c $((x + 30)) $((y + 400)) > /dev/null
  "$WTYPE" -s 300 -k F11
  check "win fullscreen" waitfor "$L" "^resized 1280x720 " 30
  "$WTYPE" -s 300 -k F11
  check "win fullscreen back" waitfor "$L" "^resized 800x500 " 30
  "$WTYPE" -s 300 r
  check "win goome_window_size" waitfor "$L" "^resized 500x360 " 30
  swaymsg -q '[app_id="goome app"] kill'
  check "win close asks the app" waitfor "$L" "^close$"
  quits $ap win
  check "win sanitizers" san_clean "$L"

  L=$O/app-csd.log
  GOOME_CSD=1 GOOME_BACKEND=wayland "$BIN/app" > "$L" 2>&1 &
  ap=$!
  waitfor "$L" "^window " 30
  sleep 0.6
  read -r x y w h <<<"$(rect)"
  check "csd geometry is window + 32 px bar ($w x $h)" test "$w $h" = "640 452"
  grim -t ppm "$O/csd.ppm"
  check "csd title bar drawn" pxis "$O/csd.ppm" $((x + 40)) $((y + 16)) "43 45 51"
  check "csd content under the bar" pxis "$O/csd.ppm" $((x + 300)) $((y + 32 + 30)) "38 42 52"
  check "csd shadow outside" bash -c "[ \$($IMG px '$O/csd.ppm' $((x + 300)) $((y + 452 + 3)) | cut -d' ' -f1) -lt \$($IMG px '$S/bg.ppm' $((x + 300)) $((y + 452 + 3)) | cut -d' ' -f1) ]"
  "$S/wl_vptr" 1280 720 c $((x + 300)) $((y + 32 + 400)) > /dev/null
  check "csd click lands in content coords" waitfor "$L" "^down 300,400 button 1$"
  "$S/wl_vptr" 1280 720 m $((x + 100)) $((y + 16)) d m $((x + 150)) $((y + 46)) w 100 m $((x + 160)) $((y + 56)) w 100 u > /dev/null
  sleep 0.4
  read -r nx ny _ _ <<<"$(rect)"
  check "csd drag the title bar ($x,$y -> $nx,$ny)" test "$nx $ny" = "$((x + 60)) $((y + 40))"
  x=$nx y=$ny
  if [ "$OLD_SWAY" = 1 ]; then
    echo "info csd edge resize skipped, sway < 1.10 doesnt send clicks outside the window geometry to the window"
  else
    "$S/wl_vptr" 1280 720 m $((x + 644)) $((y + 200)) d m $((x + 700)) $((y + 200)) w 100 m $((x + 724)) $((y + 200)) w 100 u > /dev/null
    check "csd drag the right edge resizes" waitfor "$L" "^resized 720x420 " 30
    sleep 0.3
  fi
  read -r x y w h <<<"$(rect)"
  "$S/wl_vptr" 1280 720 c $((x + w - 8 - 12)) $((y + 16)) > /dev/null
  check "csd close button" waitfor "$L" "^close$"
  quits $ap csd
  check "csd sanitizers" san_clean "$L"
  L=$O/two.log
  GOOME_BACKEND=wayland "$BIN/win_two" > "$L" 2>&1 &
  ap=$!
  waitfor "$L" "^window 1 " 30
  check "two windows in one process" grep -q "^window 0 goome one 300x200$" "$L"
  sleep 0.5
  check "two windows both mapped" test "$(swaymsg -t get_tree | grep -c '"app_id": "goome \(one\|two\)"')" = 2
  swaymsg -q '[app_id="goome two"] kill'
  check "two windows close the second" waitfor "$L" "^close 1$"
  sleep 0.3
  check "two windows first still there" test "$(swaymsg -t get_tree | grep -c '"app_id": "goome one"')" = 1
  swaymsg -q '[app_id="goome one"] kill'
  quits $ap two
  check "two windows sanitizers" san_clean "$L"
  pkill -f "$WLCOPY" 2>/dev/null
  stop_sway
}

# weston has no xdg-decoration, so goome draws its own title bar. no input devices there, so this only looks
weston_run() {
  local rt=$S/wrt L=$O/app-weston.log
  rm -rf "$rt"; mkdir -p "$rt"; chmod 700 "$rt"
  XDG_RUNTIME_DIR=$rt "$WESTON" --backend=headless --renderer=pixman --socket=wl-goome --width=1280 --height=720 --debug --no-config > "$O/weston.log" 2>&1 &
  local wp=$!
  for _ in $(seq 50); do [ -S "$rt/wl-goome" ] && break; sleep 0.1; done
  sleep 1
  XDG_RUNTIME_DIR=$rt WAYLAND_DISPLAY=wl-goome GOOME_BACKEND=wayland "$BIN/app" > "$L" 2>&1 &
  local ap=$!
  waitfor "$L" "^window " 30
  check "weston open" grep -q "^window 640x420 scale 1.00" "$L"
  sleep 1
  rm -f "$O"/wayland-screenshot*.png
  (cd "$O" && XDG_RUNTIME_DIR=$rt WAYLAND_DISPLAY=wl-goome "$WESTON_SHOT" > /dev/null 2>&1)
  local shot; shot=$(ls "$O"/wayland-screenshot*.png 2>/dev/null | head -1)
  [ -n "$shot" ] && cp "$shot" "$O/weston-csd.png"
  check "weston csd bar, title and close button" python3 - "$shot" <<'PY'
import sys, zlib, struct
d = open(sys.argv[1], "rb").read()
w, h = struct.unpack(">II", d[16:24])
raw, i = b"", 8
while i < len(d):
    n, t = struct.unpack(">I4s", d[i:i + 8])
    if t == b"IDAT": raw += d[i + 8:i + 8 + n]
    i += 12 + n
raw = zlib.decompress(raw)
bpp = 4 if d[25] == 6 else 3
st = w * bpp
rows, prev = [], bytearray(st)
p = 0
for y in range(h):
    f, line = raw[p], bytearray(raw[p + 1:p + 1 + st]); p += 1 + st
    for x in range(st):
        a = line[x - bpp] if x >= bpp else 0
        b, c = prev[x], prev[x - bpp] if x >= bpp else 0
        if f == 1: line[x] = (line[x] + a) & 255
        elif f == 2: line[x] = (line[x] + b) & 255
        elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
        elif f == 4:
            pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
            line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    rows.append(line); prev = line
px = lambda x, y: tuple(rows[y][x * bpp:x * bpp + 3])
top = next(((x, y) for y in range(h) for x in range(w) if px(x, y) == (38, 42, 52)), None)
if not top: sys.exit("no app top bar")
x, y = top
bar = px(x + 20, y - 16)
title = max(sum(px(xx, yy)) for xx in range(x + 250, x + 390) for yy in range(y - 26, y - 6))
close = max(sum(px(xx, yy)) for xx in range(x + 600, x + 630) for yy in range(y - 26, y - 6))
ok = bar in ((43, 45, 51), (34, 35, 39)) and title > 300 and close > 300
print("bar", bar, "title", title, "close", close) if not ok else None
sys.exit(0 if ok else 1)
PY
  kill $ap $wp 2>/dev/null; wait $ap $wp 2>/dev/null
  check "weston sanitizers" san_clean "$L"
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
if [ "${WL_ONLY:-}" != win ]; then
  opens_clean
  hello_run scale1 1 "output HEADLESS-1 resolution 1280x720 position 0 0" 1280 720
  hello_run scale2 2 "output HEADLESS-1 resolution 2560x1440 position 0 0 scale 2" 1280 720
  hello_run rot90 1 "output HEADLESS-1 resolution 1280x720 position 0 0 transform 90" 720 1280
  hello_run rot270flip-scale2 2 "output HEADLESS-1 resolution 2560x1440 position 0 0 scale 2 transform flipped-270" 720 1280
  watch_run
  hypr_run
fi
if [ -x "$WLCOPY" ]; then win_run; else echo "skip window mode, no wl-clipboard"; fi
WESTON=${WESTON:-$(command -v weston)} WESTON_SHOT=${WESTON_SHOT:-$(command -v weston-screenshooter)}
if [ -n "$WESTON" ] && [ -n "$WESTON_SHOT" ]; then weston_run; else echo "skip client side title bar on weston, no weston"; fi
stop_sway
[ "$nfail" = 0 ] && rm -f "$O"/*.ppm
echo "$npass passed, $nfail failed"
[ "$nfail" = 0 ]
