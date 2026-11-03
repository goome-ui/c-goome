#!/usr/bin/env bash
# win32 backend under wine on its own Xvfb, so nothing shows on the real desktop
# ./tests/win32.sh   (builds build-win first). real windows runs tests/win32.ps1
set -u
cd "$(dirname "$0")/.."
root=$PWD
scratch=${W32_SCRATCH:-$PWD/test-out/win32}
mkdir -p "$scratch/tmp" "$scratch/run"
export TMPDIR=$scratch/tmp WINEPREFIX=$scratch/wineprefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" DISPLAY=:94
unset WAYLAND_DISPLAY
ws=$(command -v wineserver || echo /usr/lib/x86_64-linux-gnu/wine/wineserver)
py=${PY:-python3}
export PYTHONWARNINGS=ignore
out=$scratch/run
B=$root/build-win
fails=0
ok() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fails=$((fails + 1)); }

./build.sh win >/dev/null || { echo "FAIL build"; exit 1; }
cc=${CC:-x86_64-w64-mingw32-gcc}
for t in hide hit; do
  "$cc" -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -O2 -Iinclude -o "$B/win32_$t.exe" tests/win32_$t.c "$B/libgoome.a" || bad build-$t
done
dlls=$(x86_64-w64-mingw32-objdump -p "$B/hello.exe" | awk '/DLL Name/{print $3}' | sort -u | tr '\n' ' ')
case "$dlls" in "KERNEL32.dll msvcrt.dll USER32.dll ") ok "imports: $dlls" ;; *) bad "imports: $dlls" ;; esac

Xvfb :94 -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 &
xp=$!
sleep 1
cleanup() {
  "$ws" -k 2>/dev/null
  kill $xp 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT
[ -d "$WINEPREFIX/drive_c" ] || { wineboot -i >/dev/null 2>&1; "$ws" -w; }
setdpi() { wine reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d "$1" /f >/dev/null 2>&1; "$ws" -w; }
setdpi 96

shot() { import -window root "$out/$1.png"; }
px() { "$py" -c "from PIL import Image; im=Image.open('$out/$1.png').convert('RGB'); print('%d %d %d' % im.getpixel(($2,$3)))"; }
near() { # near "r g b" "r g b"
  "$py" -c "import sys; a=list(map(int,'$1'.split())); b=list(map(int,'$2'.split())); sys.exit(0 if max(abs(x-y) for x,y in zip(a,b))<=6 else 1)"
}
click() { xdotool mousemove "$1" "$2"; sleep 0.4; xdotool click 1; sleep 0.6; }

wine "$B/unit.exe" > "$out/unit.txt" 2>&1
grep -q "all good" "$out/unit.txt" && ok unit || { bad unit; cat "$out/unit.txt"; }

# something underneath that logs clicks
xev -event button -geometry 1280x720+0+0 > "$out/xev.txt" 2>&1 &
xe=$!
sleep 1
wine "$B/hello.exe" > "$out/hello.txt" 2>&1 &
hp=$!
sleep 4

# 1 open
grep -q "^backend: win32 + win32" "$out/hello.txt" && grep -Eq "^monitor 0: .* 1280x720 at 0,0 scale 1.00" "$out/hello.txt" && ok open || { bad open; cat "$out/hello.txt"; }
grep -q "no global hotkeys" "$out/hello.txt" && bad "hotkey register" || ok "hotkey register"

# 2 draw, outside shot
shot draw
near "$(px draw 81 140)" "128 144 160" && near "$(px draw 150 100)" "28 35 49" && ok "draw (outside shot)" || bad "draw (outside shot) frame=$(px draw 81 140) fill=$(px draw 150 100)"

# 2 draw, our own grab vs the outside shot
wine "$B/grab.exe" 0 80 80 280 120 "$out/box.ppm" > "$out/grab1.txt" 2>&1
"$py" - "$out/draw.png" "$out/box.ppm" <<'E' && ok "draw (own grab)" || bad "draw (own grab) $(cat "$out/grab1.txt")"
import sys
from PIL import Image
a = Image.open(sys.argv[1]).convert('RGB').crop((80, 80, 360, 200)); b = Image.open(sys.argv[2]).convert('RGB')
diff = sum(1 for p, q in zip(a.getdata(), b.getdata()) if max(abs(x - y) for x, y in zip(p, q)) > 6) if a.size == b.size else -1
print('box grab', b.size, 'pixels off', diff); sys.exit(0 if a.size == b.size and diff < 200 else 1)
E

# 3 click-through
: > "$out/xev.txt"
click 200 130
grep -q "^click 200,130" "$out/hello.txt" && ok "click on the box reaches hello" || bad "click on the box reaches hello"
grep -q ButtonPress "$out/xev.txt" && bad "click on the box leaked underneath" || ok "click on the box stays with hello"
click 600 400
grep -q ButtonPress "$out/xev.txt" && grep -q "(600,400)" "$out/xev.txt" && ok "click next to it reaches the window under" || bad "click next to it reaches the window under"
grep -q "^click 600,400" "$out/hello.txt" && bad "hello saw the outside click" || ok "hello didnt see the outside click"
# 4 keys
click 200 174
xdotool type --delay 80 "Hi"; sleep 0.5
xdotool key Left; sleep 0.5
grep -q "key 104 mods 100 text 'H'" "$out/hello.txt" && grep -q "key 105 mods 0 text 'i'" "$out/hello.txt" && ok "typed text" || bad "typed text"
grep -q "key 144 mods 0 text ''" "$out/hello.txt" && ok "arrow key" || bad "arrow key"
shot typed

# 8 hotkey
xdotool key alt+h; sleep 0.8
shot hidden
near "$(px hidden 150 100)" "28 35 49" && bad "alt+h hides the box" || ok "alt+h hides the box"
xdotool key alt+h; sleep 0.8
shot shown
# wine only hands keys to a focused wine window and the hidden overlay had the focus, so this one is info here
near "$(px shown 150 100)" "28 35 49" && echo "info alt+h brought it back" || echo "info alt+h didnt bring it back under wine (no focused wine window left), win32.ps1 checks it for real"
kill $hp 2>/dev/null
kill $xe 2>/dev/null

# 3c a hit rect bigger than the drawing: its empty part still takes clicks
xev -event button -geometry 1280x720+0+0 > "$out/xev2.txt" 2>&1 &
xe=$!
sleep 1
wine "$B/win32_hit.exe" 9 > "$out/hit.txt" 2>&1 &
tp=$!
sleep 4
: > "$out/xev2.txt"
click 300 200
grep -q "^click 300,200" "$out/hit.txt" && ok "click on an empty part of a hit reaches us" || bad "click on an empty part of a hit reaches us"
grep -q ButtonPress "$out/xev2.txt" && bad "empty part of a hit leaked underneath" || ok "empty part of a hit stays with us"
click 500 400
grep -q "(500,400)" "$out/xev2.txt" && ok "click outside the hit still goes through" || bad "click outside the hit still goes through"
grep -q "alpha at the empty spot 0, on the box 255" "$out/hit.txt" && ok "alpha 1 trick doesnt leak into goome_alpha_at" || bad "alpha leak: $(head -1 "$out/hit.txt")"
wait $tp
kill $xe 2>/dev/null

# 5 capture: a known picture on screen, region + full grab, mouse parked on it
"$py" - "$out/known.png" <<'E'
import sys, random
from PIL import Image
random.seed(7); im = Image.new('RGB', (400, 300))
im.putdata([(random.randrange(256), random.randrange(256), random.randrange(256)) if (x // 20 + y // 20) % 2 else ((x * 5) % 256, (y * 3) % 256, 90) for y in range(300) for x in range(400)])
im.save(sys.argv[1])
E
display -geometry +98+48 "$out/known.png" &
dp=$!
sleep 2
xdotool mousemove 300 200
sleep 0.3
wine "$B/grab.exe" 0 100 50 400 300 "$out/region.ppm" > "$out/grab2.txt" 2>&1
wine "$B/grab.exe" "$out/full.ppm" > "$out/grab3.txt" 2>&1
shot capture
kill $dp
"$py" - "$out" <<'E' && ok "capture region + full exact" || bad "capture region + full exact"
import sys
from PIL import Image
d = sys.argv[1]
k = Image.open(d + '/known.png').convert('RGB')
r = Image.open(d + '/region.ppm').convert('RGB'); f = Image.open(d + '/full.ppm').convert('RGB'); s = Image.open(d + '/capture.png').convert('RGB')
fc = f.crop((100, 50, 500, 350))
okr = r.size == k.size and list(r.getdata()) == list(k.getdata())
okf = f.size == (1280, 720) and list(fc.getdata()) == list(k.getdata())
oks = list(f.getdata()) == list(s.getdata())
print('region', r.size, okr, 'full', f.size, okf, 'full == outside shot', oks)
sys.exit(0 if okr and okf and oks else 1)
E
grep -q "has the cursor" "$out/grab2.txt" "$out/grab3.txt" && bad "cursor not in grabs" || ok "cursor not in grabs"

# hide_self: wine has no capture exclusion, this only shows what it says
wine "$B/win32_hide.exe" > "$out/hide.txt" 2>&1
echo "info hide_self under wine: $(cat "$out/hide.txt")"

# 7 scale: 144 dpi, the box lands on real px 120,120 and clicks map back
setdpi 144
wine "$B/hello.exe" > "$out/hello2.txt" 2>&1 &
hp=$!
sleep 4
shot scaled
grep -Eq "^monitor 0: .* 853x480 at 0,0 scale 1.50" "$out/hello2.txt" && ok "scale 1.5 monitor" || bad "scale 1.5 monitor: $(grep monitor "$out/hello2.txt")"
near "$(px scaled 121 210)" "128 144 160" && near "$(px scaled 225 150)" "28 35 49" && ok "scale 1.5 box on real px" || bad "scale 1.5 box on real px frame=$(px scaled 121 210) fill=$(px scaled 225 150)"
click 300 195
grep -q "^click 200,130" "$out/hello2.txt" && ok "scale 1.5 click maps back" || bad "scale 1.5 click maps back: $(grep click "$out/hello2.txt")"
click 514 135 # the x at logical 343,90
sleep 1
kill -0 $hp 2>/dev/null && { bad "x quits hello"; kill $hp; } || ok "x quits hello"
"$ws" -k
setdpi 96

echo "info hotplug: Xvfb has one fixed screen, wine cant hotplug here, see win32.ps1 / by hand"
[ $fails = 0 ] && echo "win32 wine: all passed" || echo "win32 wine: $fails failed"
exit $((fails != 0))
