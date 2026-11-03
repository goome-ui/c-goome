#!/usr/bin/env bash
# x11 backend on Xvfb :91, once with picom and once without a compositor
#   tests/x11.sh          uses build/ (run ./build.sh first)
#   tests/x11.sh asan     builds hello, grab and the probe with asan+ubsan first
# heavy (Xvfb + picom), takes a few minutes
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
S=${X11_SCRATCH:-$PWD/test-out/x11}
T=${X11_TOOLS:-}
DPY=${X11_DISPLAY:-:91}
rm -rf "$S" && mkdir -p "$S"
cd "$S" || exit 1
export TMPDIR=$S
unset WAYLAND_DISPLAY XDG_CURRENT_DESKTOP GOOME_BACKEND
export XDG_SESSION_TYPE=x11

cflags=(-std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -I"$repo/include" -I"$repo/src")
if [ "${1:-}" = asan ]; then
  cflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:exitcode=99 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:exitcode=98
  printf 'leak:libX11.so\nleak:libxcb.so\nleak:libXfixes.so\nleak:libXrandr.so\nleak:libXext.so\n' > lsan.supp
  export LSAN_OPTIONS=suppressions=$S/lsan.supp
fi
mkdir -p bin
for f in "$repo"/examples/hello.c "$repo"/examples/grab.c "$repo"/tests/x11_probe.c; do
  cc "${cflags[@]}" -o "bin/$(basename "${f%.c}")" "$f" "$repo"/src/*.c || exit 1
done
B=$S/bin

pass=0 fail=0
ok() { echo "PASS $*"; pass=$((pass + 1)); }
no() { echo "FAIL $*"; fail=$((fail + 1)); }
check() { local name=$1; shift; if "$@"; then ok "$name"; else no "$name"; fi; }
waitfor() { for _ in $(seq 1 ${3:-30}); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done; return 1; }

cat > px.py <<'EOF'
import sys
def ppm(path):
    d = open(path, 'rb').read()
    parts, i = [], 0
    while len(parts) < 4:
        while d[i:i+1].isspace(): i += 1
        if d[i:i+1] == b'#':
            while d[i:i+1] != b'\n': i += 1
            continue
        j = i
        while not d[j:j+1].isspace(): j += 1
        parts.append(d[i:j]); i = j
    w, h = int(parts[1]), int(parts[2])
    return w, h, d[i+1:i+1+w*h*3]
def px(img, x, y):
    w, h, b = img
    k = (y * w + x) * 3
    return b[k], b[k+1], b[k+2]
cmd = sys.argv[1]
if cmd == 'get':
    print('%d %d %d' % px(ppm(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])))
elif cmd == 'near':
    r = px(ppm(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]))
    want = [int(v) for v in sys.argv[5:8]]
    sys.exit(0 if all(abs(a - b) <= 4 for a, b in zip(r, want)) else (print('got', r, 'want', want) or 1))
elif cmd == 'same':
    a, b = ppm(sys.argv[2]), ppm(sys.argv[3])
    ox, oy = int(sys.argv[4]), int(sys.argv[5])
    skip = [tuple(int(v) for v in s.split(',')) for s in sys.argv[6:]]
    bad = 0
    for y in range(a[1]):
        for x in range(a[0]):
            gx, gy = x + ox, y + oy
            if any(sx <= gx < sx + sw and sy <= gy < sy + sh for sx, sy, sw, sh in skip): continue
            if px(a, x, y) != px(b, gx, gy):
                if bad < 3: print('differs at', gx, gy, px(a, x, y), px(b, gx, gy))
                bad += 1
    sys.exit(1 if bad else 0)
elif cmd == 'pattern':
    w, h = int(sys.argv[3]), int(sys.argv[4])
    out = bytearray()
    for y in range(h):
        for x in range(w):
            out += bytes(((x * 7 + y * 3) & 255, (x ^ y) & 255, (x * y // 64 + 40) & 255))
    open(sys.argv[2], 'wb').write(b'P6\n%d %d\n255\n' % (w, h) + out)
EOF

cat > mons.py <<'EOF'
# mons.py set NAME X W [out] / mons.py del NAME: fake randr monitors, out = takes over the real output. needs Xvfb -noreset
import ctypes, sys
x = ctypes.CDLL("libX11.so.6"); r = ctypes.CDLL("libXrandr.so.2")
class M(ctypes.Structure):
    _fields_ = [("name", ctypes.c_ulong), ("primary", ctypes.c_int), ("automatic", ctypes.c_int), ("noutput", ctypes.c_int),
                ("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
                ("mwidth", ctypes.c_int), ("mheight", ctypes.c_int), ("outputs", ctypes.POINTER(ctypes.c_ulong))]
x.XOpenDisplay.restype = ctypes.c_void_p
x.XDefaultRootWindow.restype = ctypes.c_ulong; x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x.XInternAtom.restype = ctypes.c_ulong; x.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
r.XRRGetMonitors.restype = ctypes.POINTER(M); r.XRRGetMonitors.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
r.XRRAllocateMonitor.restype = ctypes.POINTER(M); r.XRRAllocateMonitor.argtypes = [ctypes.c_void_p, ctypes.c_int]
r.XRRSetMonitor.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(M)]
r.XRRDeleteMonitor.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong]
d = x.XOpenDisplay(None); root = x.XDefaultRootWindow(d)
if sys.argv[1] == "del":
    r.XRRDeleteMonitor(d, root, x.XInternAtom(d, sys.argv[2].encode(), 0))
else:
    n = ctypes.c_int(); cur = r.XRRGetMonitors(d, root, 1, ctypes.byref(n))
    out = [cur[i].outputs[0] for i in range(n.value) if cur[i].noutput][0]
    outs = 1 if sys.argv[5:] else 0
    m = r.XRRAllocateMonitor(d, outs)
    m.contents.name = x.XInternAtom(d, sys.argv[2].encode(), 0)
    m.contents.x, m.contents.y, m.contents.width, m.contents.height = int(sys.argv[3]), 0, int(sys.argv[4]), 1080
    m.contents.mwidth, m.contents.mheight = 500, 280
    if outs: m.contents.outputs[0] = out
    r.XRRSetMonitor(d, root, m)
x.XSync(d, 0)
EOF

cat > key.py <<'EOF'
# key.py ctrl+shift+F5 ...: real keycodes through xtest, xdotool adds alt to F keys on Xvfb (it finds them on the ctrl+alt VT switch level)
import ctypes, sys, time
x = ctypes.CDLL("libX11.so.6"); t = ctypes.CDLL("libXtst.so.6")
x.XOpenDisplay.restype = ctypes.c_void_p
x.XStringToKeysym.restype = ctypes.c_ulong; x.XStringToKeysym.argtypes = [ctypes.c_char_p]
x.XKeysymToKeycode.restype = ctypes.c_ubyte; x.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
t.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
x.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
d = x.XOpenDisplay(None)
names = {"ctrl": "Control_L", "shift": "Shift_L", "alt": "Alt_L", "super": "Super_L"}
for combo in sys.argv[1:]:
    kcs = [x.XKeysymToKeycode(d, x.XStringToKeysym(names.get(k, k).encode())) for k in combo.split("+")]
    for k in kcs: t.XTestFakeKeyEvent(d, k, 1, 0)
    for k in reversed(kcs): t.XTestFakeKeyEvent(d, k, 0, 0)
    x.XSync(d, 0); time.sleep(0.05)
EOF
python3 px.py pattern pat.ppm 1280 720

xpid=
stop_x() { pkill -P $$ -f 'xev|display|picom|hello|probe' 2>/dev/null; [ -n "$xpid" ] && kill "$xpid" 2>/dev/null; wait 2>/dev/null; xpid=; }
start_x() { # size, compositor 0/1
  Xvfb "$DPY" -screen 0 "$1"x24 -nolisten tcp -noreset >"xvfb-$tag.log" 2>&1 &
  xpid=$!
  export DISPLAY=$DPY
  for _ in $(seq 1 50); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
  if [ "$2" = 1 ]; then
    pc=$(command -v picom || echo "$T/usr/bin/picom")
    LD_LIBRARY_PATH=$T/usr/lib/x86_64-linux-gnu "$pc" --backend xrender --config /dev/null --no-fading-openclose >"picom-$tag.log" 2>&1 &
    sleep 1
  fi
}
shot() { import -window root -depth 8 "ppm:$1"; }
P() { python3 "$S/px.py" "$@"; }
P2() { python3 "$S/$@"; }

run_set() {
  tag=$1
  local comp=$([ "$tag" = picom ] && echo 1 || echo 0)
  echo "== $tag"
  start_x 1280x720 "$comp"
  display -geometry +0+0 -borderwidth 0 pat.ppm &
  sleep 1
  xev -geometry 300x200+900+450 -event button >"xev-$tag.log" 2>&1 &
  sleep 0.5
  "$B/hello" >"hello-$tag.log" 2>"hello-$tag.err" &
  local hp=$!
  waitfor "hello-$tag.log" "monitor 0" 40
  sleep 0.7

  check "$tag open: backend name" grep -q '^backend: x11 + x11$' "hello-$tag.log"
  check "$tag open: one 1280x720 monitor, scale 1" grep -q '^monitor 0: .* 1280x720 at 0,0 scale 1.00$' "hello-$tag.log"
  if [ "$comp" = 1 ]; then check "$tag open: sees the compositor" bash -c "! grep -q 'no compositor' hello-$tag.err"
  else check "$tag open: warns about no compositor" grep -q 'no compositor' "hello-$tag.err"; fi

  shot "shot-$tag.ppm"
  # box fill 0xE0202838 premultiplied = 28,35,49, plus 31/255 of what is under it when composited
  read -r pr pg pb <<<"$(P get pat.ppm 300 150)"
  if [ "$comp" = 1 ]; then want="$((28 + pr * 31 / 255)) $((35 + pg * 31 / 255)) $((49 + pb * 31 / 255))"; else want="28 35 49"; fi
  check "$tag draw: box fill on screen" P near "shot-$tag.ppm" 300 150 $want
  check "$tag draw: rest of the screen untouched" P same "shot-$tag.ppm" pat.ppm 0 0 80,80,280,120 895,445,315,215
  "$B/grab" 0 80 80 280 120 "own-$tag.ppm" >"grab-$tag.log" 2>&1
  check "$tag draw: own grab of the box matches the screenshot" P same "own-$tag.ppm" "shot-$tag.ppm" 80 80
  check "$tag draw: grab says it has our overlay" grep -q 'has our overlay' "grab-$tag.log"

  xdotool mousemove 300 150 click 1
  check "$tag click: on the box reaches hello" waitfor "hello-$tag.log" '^click 300,150$'
  xdotool mousemove 1000 520 click 1
  sleep 0.3
  check "$tag click: next to it reaches the window under" grep -q 'root:(1000,520).*' "xev-$tag.log"
  check "$tag click: hello didnt get that one" bash -c "! grep -q '^click 1000,520' hello-$tag.log"

  xdotool mousemove 200 174 click 1
  waitfor "hello-$tag.log" '^click 200,174$'
  sleep 0.2
  xdotool type --delay 40 'Hi x!'
  P2 key.py Left F5 BackSpace
  sleep 0.4
  check "$tag keys: shift+h" grep -q "^key 104 mods 100 text 'H'$" "hello-$tag.log"
  check "$tag keys: plain i" grep -q "^key 105 mods 0 text 'i'$" "hello-$tag.log"
  check "$tag keys: space" grep -q "^key 32 mods 0 text ' '$" "hello-$tag.log"
  check "$tag keys: shift+1 = !" grep -q "^key 49 mods 100 text '!'$" "hello-$tag.log"
  check "$tag keys: left arrow" grep -q "^key 144 mods 0 text ''$" "hello-$tag.log"
  check "$tag keys: f5" grep -q "^key 133 mods 0 text ''$" "hello-$tag.log"
  check "$tag keys: backspace" grep -q "^key 8 mods 0 text ''$" "hello-$tag.log"
  shot "typed-$tag.ppm"
  xdotool mousemove 200 174 click 1
  sleep 0.3
  xdotool type z
  sleep 0.3
  check "$tag keys: keyboard off = no more keys" bash -c "! grep -q \"^key 122 \" hello-$tag.log"

  xdotool mousemove 600 150
  sleep 0.2
  "$B/grab" 0 500 100 300 200 "reg-$tag.ppm" >/dev/null 2>&1
  check "$tag capture: region grab is the picture, no cursor" P same "reg-$tag.ppm" pat.ppm 500 100
  "$B/grab" "full-$tag.ppm" >/dev/null 2>&1
  check "$tag capture: full grab is the picture outside box and xev" P same "full-$tag.ppm" pat.ppm 0 0 80,80,280,120 895,445,315,215
  check "$tag capture: full grab = screenshot" P same "full-$tag.ppm" "shot-$tag.ppm" 0 0 80,80,280,120

  xdotool key alt+h
  sleep 0.4
  shot "hid-$tag.ppm"
  check "$tag hotkey: alt+h hides the box" P near "hid-$tag.ppm" 300 150 $(P get pat.ppm 300 150)
  xdotool key Num_Lock
  xdotool key alt+h
  sleep 0.4
  xdotool key Num_Lock
  shot "back-$tag.ppm"
  check "$tag hotkey: alt+h with numlock on shows it again" P near "back-$tag.ppm" 300 150 $want

  xdotool mousemove 342 96 click 1
  for _ in $(seq 1 30); do kill -0 $hp 2>/dev/null || break; sleep 0.1; done
  if kill -0 $hp 2>/dev/null; then no "$tag quit: the x closes hello"; kill $hp; else wait $hp; rc=$?; check "$tag quit: the x closes hello cleanly (rc $rc)" test $rc = 0; fi
  check "$tag asan/ubsan: hello clean" bash -c "! grep -qE 'ERROR: (Address|Leak)Sanitizer|runtime error' hello-$tag.err"
  stop_x

  echo "== $tag, two monitors"
  start_x 3840x1080 "$comp"
  P2 mons.py set LEFT-1 0 1920 out
  P2 mons.py set RIGHT-1 1920 1920
  "$B/x11_probe" >"probe-$tag.log" 2>"probe-$tag.err" &
  local pp=$!
  waitfor "probe-$tag.log" '^hotkey' 40
  sleep 0.7
  check "$tag monitors: two from randr" grep -q '^monitors 2$' "probe-$tag.log"
  check "$tag monitors: LEFT-1 at 0,0" grep -q '^mon 0 LEFT-1 0,0 1920x1080 scale 1.00 px 1920x1080$' "probe-$tag.log"
  check "$tag monitors: RIGHT-1 at 1920,0" grep -q '^mon 1 RIGHT-1 1920,0 1920x1080 scale 1.00 px 1920x1080$' "probe-$tag.log"
  check "$tag monitors: caps global pointer + keyboard + hotkeys + capture" grep -q '^backend x11 + x11 caps 17$' "probe-$tag.log"
  check "$tag hotkey: ctrl+shift+f5 grabbed" grep -q '^hotkey 0$' "probe-$tag.log"
  shot "two-$tag.ppm"
  check "$tag monitors: box on the left one" P near "two-$tag.ppm" 150 130 0 192 0
  check "$tag monitors: box on the right one" P near "two-$tag.ppm" 2070 130 0 192 0
  check "$tag monitors: nothing between them" P near "two-$tag.ppm" 1000 130 0 0 0
  "$B/grab" 1 "mon1-$tag.ppm" >"grab1-$tag.log" 2>&1
  check "$tag capture: monitor 1 grab is 1920x1080 of 1920,0" grep -q '^1920x1080 px of 1920,0 1920x1080 on monitor 1' "grab1-$tag.log"
  check "$tag capture: monitor 1 grab has its box" P near "mon1-$tag.ppm" 150 130 0 192 0
  xdotool mousemove 2070 130 click 1
  check "$tag click: right monitor box, mon 1" waitfor "probe-$tag.log" '^down 2070,130 mon 1 button 1$'
  check "$tag click: up comes too" waitfor "probe-$tag.log" '^up 2070,130 button 1$'
  xdotool click 2
  check "$tag click: middle button" waitfor "probe-$tag.log" '^down 2070,130 mon 1 button 2$'
  xdotool click 4
  check "$tag scroll: wheel up = -1" waitfor "probe-$tag.log" '^scroll -1 at 2070,130$'
  xdotool click 5
  check "$tag scroll: wheel down = +1" waitfor "probe-$tag.log" '^scroll 1 at 2070,130$'
  xdotool click 6
  xdotool mousemove 2100 140
  check "$tag motion: over the box" waitfor "probe-$tag.log" '^motion 2100,140$'
  xdotool mousemove 3000 700
  sleep 0.3
  check "$tag motion: none off the box" bash -c "! grep -q '^motion 3000,700' probe-$tag.log"
  check "$tag pointer: global position off the box" waitfor "probe-$tag.log" '^ptr 3000,700 0$'
  xdotool mousedown 1
  check "$tag pointer: held button shows" waitfor "probe-$tag.log" '^ptr 3000,700 1$'
  xdotool mouseup 1
  P2 key.py ctrl+shift+F5
  check "$tag hotkey: ctrl+shift+f5 fires id 7" waitfor "probe-$tag.log" '^hotkey id 7$'
  P2 key.py Caps_Lock ctrl+shift+F5 Caps_Lock
  sleep 0.3
  check "$tag hotkey: still fires with capslock on" test "$(grep -c '^hotkey id 7$' "probe-$tag.log")" = 2
  xdotool mousemove 150 130 click 3
  check "$tag keys: keyboard grab works" waitfor "probe-$tag.log" '^keyboard 0$'
  xdotool key --delay 30 ctrl+a Home Escape
  check "$tag keys: ctrl+a" waitfor "probe-$tag.log" "^key 97 mods 200 text ''$"
  check "$tag keys: home" waitfor "probe-$tag.log" "^key 148 mods 0 text ''$"

  P2 mons.py del RIGHT-1
  check "$tag hotplug: drops to one monitor" waitfor "probe-$tag.log" '^monitors 1$' 50
  sleep 0.5
  shot "one-$tag.ppm"
  check "$tag hotplug: right box gone" P near "one-$tag.ppm" 2070 130 0 0 0
  check "$tag hotplug: one box left" P near "one-$tag.ppm" 150 130 0 192 0
  P2 mons.py set RIGHT-1 1920 1280
  check "$tag hotplug: two again, new layout" waitfor "probe-$tag.log" '^mon 1 RIGHT-1 1920,0 1280x1080' 50
  sleep 0.5
  shot "re-$tag.ppm"
  check "$tag hotplug: box on the new right monitor" P near "re-$tag.ppm" 2070 130 0 192 0
  kill -0 $pp 2>/dev/null && check "$tag hotplug: probe still alive" true || no "$tag hotplug: probe still alive"
  xdotool mousemove 2070 130 click 3
  waitfor "probe-$tag.log" '^down 2070,130 mon 1 button 3$'
  sleep 0.2
  xdotool key q
  for _ in $(seq 1 30); do kill -0 $pp 2>/dev/null || break; sleep 0.1; done
  kill -0 $pp 2>/dev/null && { no "$tag quit: probe"; kill $pp; } || { wait $pp; rc=$?; check "$tag quit: probe exits cleanly (rc $rc)" test $rc = 0; }
  check "$tag asan/ubsan: probe clean" bash -c "! grep -qE 'ERROR: (Address|Leak)Sanitizer|runtime error' probe-$tag.err"
  stop_x
}

echo "== no display"
(unset DISPLAY; GOOME_BACKEND=x11 "$B/hello" >nodpy.log 2>&1); rc=$?
check "open: no DISPLAY fails cleanly (rc $rc)" test $rc = 1
(DISPLAY=:79 GOOME_BACKEND=x11 "$B/hello" >deaddpy.log 2>&1); rc=$?
check "open: dead DISPLAY fails cleanly (rc $rc)" test $rc = 1

run_set nocomp
run_set picom

echo "== grab over tcp (no shared memory, plain XGetImage)"
Xvfb "$DPY" -screen 0 1280x720x24 -listen tcp -ac -noreset >xvfb-tcp.log 2>&1 &
xpid=$!
sleep 1
export DISPLAY=localhost$DPY
display -geometry +0+0 -borderwidth 0 pat.ppm &
sleep 1
"$B/grab" 0 0 0 1280 720 tcp.ppm >grab-tcp.log 2>&1
check "capture: tcp full grab is the picture" P same tcp.ppm pat.ppm 0 0
"$B/grab" 0 100 50 64 33 tcp2.ppm >/dev/null 2>&1
check "capture: tcp odd sized region grab" P same tcp2.ppm pat.ppm 100 50
stop_x

echo "passed $pass, failed $fail"
[ "$fail" = 0 ]
