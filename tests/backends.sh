#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
out="${BACKENDS_OUT:-$PWD/build/backends}"
pc="${PKG_CONFIG:-pkg-config}"
mkdir -p "$out"
warn=(-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror -O1 -Iinclude -Ibackends)
[ -f build/libgoome.a ] || ./build.sh >/dev/null || exit 1

Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$out/display" >"$out/xvfb.log" 2>&1 &
xvfb=$!
trap 'kill $xvfb 2>/dev/null' EXIT
for _ in $(seq 300); do [ -s "$out/display" ] && break; sleep 0.1; done
[ -s "$out/display" ] || { echo "Xvfb didnt start"; cat "$out/xvfb.log"; exit 1; }
export DISPLAY=":$(cat "$out/display")" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY

fails=0
ran=0
for t in glfw_gl:glfw3 sdl2_gl:sdl2 sdl3_gl:sdl3; do
  name=${t%%:*} pkg=${t#*:}
  if ! command -v "$pc" >/dev/null; then echo "skip $name: no pkg-config"; continue; fi
  if ! "$pc" --exists "$pkg"; then echo "skip $name: no $pkg dev files ($pc cant find $pkg)"; continue; fi
  if ! cc "${warn[@]}" $("$pc" --cflags "$pkg") -o "$out/$name" "tests/backends/$name.c" build/libgoome.a $("$pc" --libs "$pkg") -ldl; then
    echo "FAIL $name doesnt build"
    fails=$((fails + 1))
    continue
  fi
  ran=$((ran + 1))
  "$out/$name" || fails=$((fails + 1))
done

winebin=$(command -v wine || command -v wine64 || true)
if command -v x86_64-w64-mingw32-gcc >/dev/null && [ -n "$winebin" ]; then
  [ -f build-win/libgoome.a ] || ./build.sh win >/dev/null || exit 1
  if x86_64-w64-mingw32-gcc "${warn[@]}" -o "$out/win32_gl.exe" tests/backends/win32_gl.c build-win/libgoome.a -lgdi32 -lopengl32; then
    ran=$((ran + 1))
    WINEPREFIX="${WINEPREFIX:-$out/wine}" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" "$winebin" "$out/win32_gl.exe" || fails=$((fails + 1))
  else
    echo "FAIL win32_gl doesnt build"
    fails=$((fails + 1))
  fi
else
  echo "skip win32_gl: needs x86_64-w64-mingw32-gcc and wine"
fi

echo "backends: $ran ran, $fails failed"
[ "$fails" = 0 ]
