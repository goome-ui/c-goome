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
for t in grab_ui win_game win_ime; do
  "$cc" -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -O2 -Iinclude -o "$B/$t.exe" tests/$t.c "$B/libgoome.a" || bad build-$t
done
"$cc" -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -O2 -Iinclude -o "$B/winmode.exe" tests/winmode.c "$B/libgoome.a" || bad build-winmode
dlls=$(x86_64-w64-mingw32-objdump -p "$B/hello.exe" | awk '/DLL Name/{print $3}' | LC_ALL=C sort -fu | tr '\n' ' ')
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

lines() { wc -l <"$1"; }
after() { for _ in $(seq 40); do tail -n +"$(($2 + 1))" "$1" | grep -q -- "$3" && return 0; sleep 0.1; done; return 1; }
waitln() { for _ in $(seq 60); do grep -q -- "$2" "$1" && return 0; sleep 0.1; done; return 1; }
G=$out/game.txt U=$out/grab_ui.txt
wine "$B/win_game.exe" 500 300 600 350 40 > "$G" 2>&1 &
gp=$!
waitln "$G" "^game" && sleep 1
wine "$B/grab_ui.exe" 30 > "$U" 2>&1 &
up=$!
waitln "$U" "^ready" && sleep 1
caps=$(sed -n 's/^caps \([0-9a-f]*\) .*/\1/p' "$U")
[ $((0x${caps:-0} & 0x100)) != 0 ] && ok "grab cap ($caps)" || bad "grab cap ($caps)"
click 800 450
grep -q "^focus in" "$G" && ok "grab game has the focus" || bad "grab game has the focus: $(tr '\n' ' ' < "$G")"
xdotool mousemove 820 460; sleep 0.5
grep -q "^move 320,160" "$G" && ok "grab game sees the mouse first" || bad "grab game sees the mouse first"
grep -q "^raw" "$G" && echo "info wine sends raw input to the game" || echo "info no raw input under wine, win32.ps1 checks it"
n=$(lines "$G")
xdotool mousemove 118 177; sleep 0.4; xdotool mousedown 1
waitln "$U" "^grab 1" && ok "grab on" || bad "grab on: $(tr '\n' ' ' < "$U")"
after "$G" "$n" "^focus out" && ok "grab game lost the focus" || bad "grab game lost the focus"
sleep 0.3
n=$(lines "$G")
for p in "650 380" "750 420" "850 470" "950 520"; do xdotool mousemove $p; sleep 0.15; done
sleep 0.4
tail -n +$((n + 1)) "$G" | grep -qE "^(move|raw)" && bad "grab game got the mouse during the drag: $(tail -n +$((n + 1)) "$G" | head -3 | tr '\n' ' ')" || ok "grab game got no mouse during the drag"
waitln "$U" "^val 100" && ok "grab slider kept going outside the overlay" || bad "grab slider kept going outside the overlay"
n=$(lines "$G")
xdotool mouseup 1
after "$U" 0 "^grab 0" && ok "grab off on release" || bad "grab off on release"
after "$G" "$n" "^focus in" && ok "grab game has the focus back" || bad "grab game has the focus back"
xdotool mousemove 850 480; sleep 0.5
grep -q "^move 350,180" "$G" && ok "grab game sees the mouse again" || bad "grab game sees the mouse again"
kill $up $gp 2>/dev/null
wait $up $gp 2>/dev/null

# window mode: a real window, everything checked from inside and from the outside
ready() { for _ in $(seq 60); do grep -q "^ready" "$1" && return 0; sleep 0.5; done; return 1; }
wine "$B/winmode.exe" plain 60 > "$out/wm.txt" 2>&1 &
wp=$!
ready "$out/wm.txt" && ok "window ready" || { bad "window ready"; cat "$out/wm.txt"; }
for c in open frame clipboard "clipboard cut" cursor resize "canvas follows" pos minimize restore maximize "back to normal" capture; do
  grep -q "^ok $c" "$out/wm.txt" && ok "window $c" || bad "window $c: $(grep -E "^(bad )?$c" "$out/wm.txt")"
done
grep -q "^drop 50,60 'C:\\\\goome\\\\dropped é file.txt' full 'C:\\\\goome\\\\dropped é file.txt'" "$out/wm.txt" && ok "window drop" || bad "window drop: $(grep drop "$out/wm.txt")"
read -r wx wy < <(sed -n 's/^ready client \(-\?[0-9]*\),\(-\?[0-9]*\) .*/\1 \2/p' "$out/wm.txt")
wx=${wx:-0} wy=${wy:-0}
sleep 1
shot window
near "$(px window $((wx + 60)) $((wy + 60)))" "224 64 96" && near "$(px window $((wx + 400)) $((wy + 300)))" "32 48 64" && ok "window on screen" || bad "window on screen red=$(px window $((wx + 60)) $((wy + 60))) bg=$(px window $((wx + 400)) $((wy + 300)))"
[ "$(xsel -b -o 2>/dev/null)" = "$(printf 'goome clip \xc3\xa9 1\nline 2')" ] && ok "clipboard reaches x11" || bad "clipboard reaches x11: '$(xsel -b -o 2>/dev/null)'"
printf 'from x11' | xsel -b -i
sleep 1.5
grep -q "^paste 'from x11'" "$out/wm.txt" && ok "clipboard from x11" || bad "clipboard from x11"
click $((wx + 300)) $((wy + 200))
grep -q "^down 300,200 1" "$out/wm.txt" && grep -q "^up 300,200 1" "$out/wm.txt" && ok "window click" || bad "window click: $(grep -E '^(down|up)' "$out/wm.txt" | tail -2)"
xdotool type --delay 80 "Hi"; sleep 0.5
xdotool key Left; sleep 0.3
grep -q "key 104 mods 100 text 'H'" "$out/wm.txt" && grep -q "key 105 mods 0 text 'i'" "$out/wm.txt" && grep -q "key 144 mods 0" "$out/wm.txt" && ok "window keys" || bad "window keys"
xdotool key alt+F4; sleep 0.8
grep -q "^close 1" "$out/wm.txt" && kill -0 $wp 2>/dev/null && ok "alt+f4 sends close, the app stays up" || bad "alt+f4 sends close"
xdotool key alt+F4; sleep 1.5
kill -0 $wp 2>/dev/null && { bad "second close quits"; kill $wp; } || ok "second close quits"
grep -q "^bye fails 0" "$out/wm.txt" && ok "window run had no fails" || bad "window run: $(grep '^bad' "$out/wm.txt")"

wine "$B/winmode.exe" borderless 3 > "$out/wm-borderless.txt" 2>&1
grep -q "^bye fails 0" "$out/wm-borderless.txt" && ok "borderless window" || bad "borderless window: $(grep '^bad' "$out/wm-borderless.txt")"
wine "$B/winmode.exe" two 3 > "$out/wm-two.txt" 2>&1
grep -q "^ok two windows" "$out/wm-two.txt" && grep -q "^bye fails 0" "$out/wm-two.txt" && ok "two windows in one process" || bad "two windows: $(grep '^bad' "$out/wm-two.txt")"
wine "$B/winmode.exe" trans 8 > "$out/wm-trans.txt" 2>&1 &
wp=$!
ready "$out/wm-trans.txt"
read -r tx ty < <(sed -n 's/^ready client \(-\?[0-9]*\),\(-\?[0-9]*\) .*/\1 \2/p' "$out/wm-trans.txt")
tx=${tx:-0} ty=${ty:-0}
sleep 1
shot window-trans
near "$(px window-trans $((tx + 60)) $((ty + 60)))" "224 64 96" && near "$(px window-trans $((tx + 400)) $((ty + 300)))" "0 0 0" && ok "transparent window is shaped" || bad "transparent window red=$(px window-trans $((tx + 60)) $((ty + 60))) hole=$(px window-trans $((tx + 400)) $((ty + 300)))"
wait $wp

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
wine "$B/winmode.exe" plain 2 > "$out/wm-144.txt" 2>&1
grep -q "^window 640x400 scale 1.50 px 960x600" "$out/wm-144.txt" && grep -q "^ok frame" "$out/wm-144.txt" && ok "scale 1.5 window is 960x600 real px" || bad "scale 1.5 window: $(head -3 "$out/wm-144.txt")"
"$ws" -k
setdpi 96

# 8 ime: imm32 composition through wines own ime, set a string then finish it
timeout 60 wine "$B/win_ime.exe" 2>&1 | tr -d '\r' > "$out/ime.txt"
grep -q "^ime rc 0" "$out/ime.txt" && grep -q "^context yes" "$out/ime.txt" && ok "ime turns on" || bad "ime turns on: $(head -3 "$out/ime.txt")"
grep -q "^preedit 'にほん' 9$" "$out/ime.txt" && ok "ime preedit with the caret at the end" || bad "ime preedit: $(grep preedit "$out/ime.txt")"
grep -q "^text にほん$" "$out/ime.txt" && grep -q "^preedit '' 0$" "$out/ime.txt" && ok "ime commit clears the preedit" || bad "ime commit: $(grep -v preedit "$out/ime.txt")"
grep -q "^bye" "$out/ime.txt" && ok "ime off and close" || bad "ime off and close"

echo "info hotplug: Xvfb has one fixed screen, wine cant hotplug here, see win32.ps1 / by hand"
[ $fails = 0 ] && echo "win32 wine: all passed" || echo "win32 wine: $fails failed"
exit $((fails != 0))
