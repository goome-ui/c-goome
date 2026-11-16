#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
if [ $# -lt 2 ]; then
  echo "usage: web-build.sh app.c outdir [more.c ...]" >&2
  exit 2
fi
app="$1"
out="$2"
shift 2
extra=("$@")
die() { echo "web-build: $*" >&2; exit 1; }
[ -f "$app" ] || die "no such file: $app"
for f in "${extra[@]}"; do [ -f "$f" ] || die "no such file: $f"; done

srcs=("$app" "${extra[@]}")
code="$(cat "${srcs[@]}")"
if ! grep -qE 'GOOME_MODE_WINDOW|goome_open_app' <<<"$code"; then
  die "$app isnt a goome app. the browser is the window, so only apps (goome_open_app or .mode = GOOME_MODE_WINDOW) can be built for the web. overlays and mod menus draw over other programs and a web page cant do that"
fi
if ! grep -qE '(^|[^a-zA-Z_])goome_web_init[[:space:]]*\(' <<<"$code" || ! grep -qE '(^|[^a-zA-Z_])goome_web_frame[[:space:]]*\(' <<<"$code"; then
  die "$app needs int goome_web_init(void) and int goome_web_frame(void). a browser cant sit inside a while loop, so the loop body goes in goome_web_frame (return 0 to quit) and the setup before it in goome_web_init (return 0 when ok). see wasm/web/README.md"
fi

clang="${CLANG:-clang}"
command -v "$clang" >/dev/null || die "no clang, install clang and lld (apt install clang lld) or set CLANG="
"$clang" --print-targets 2>/dev/null | grep -q wasm32 || die "$clang cant target wasm32, use a newer clang or one with the webassembly target"
ld="${WASM_LD:-$(dirname "$(command -v "$clang")")/wasm-ld}"
[ -x "$ld" ] || ld="$(command -v wasm-ld || true)"
[ -n "$ld" ] || die "no wasm-ld, install lld (apt install lld) or set WASM_LD="

res="$("$clang" -print-resource-dir)"
mkdir -p "$out"
obj="$(mktemp -d "$out/.obj.XXXXXX")"
trap 'rm -rf "$obj"' EXIT
base=(--target=wasm32 -nostdlib -nostdlibinc -isystem "$here/libc/include" -isystem "$res/include" -std=c99 -Os -ffunction-sections -fdata-sections -Wall -Wextra
  -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I"$root/include" -I"$root/src" -I"$here" ${GOOME_WEB_CFLAGS:-})
objs=()
for f in goome draw image font ui a11y test offscreen; do
  "$clang" "${base[@]}" -DGOOME_WEB -c "$root/src/$f.c" -o "$obj/$f.o"
  objs+=("$obj/$f.o")
done
"$clang" "${base[@]}" -DLC_CONSOLE -c "$here/libc/libc.c" -o "$obj/libc.o"
"$clang" "${base[@]}" -DGOOME_WEB -c "$here/web/web.c" -o "$obj/web.o"
objs+=("$obj/libc.o" "$obj/web.o")
i=0
for f in "${srcs[@]}"; do
  "$clang" "${base[@]}" -I"$(dirname "$f")" -c "$f" -o "$obj/app$i.o"
  objs+=("$obj/app$i.o")
  i=$((i + 1))
done
exports=(gmw_init gmw_frame gmw_resize gmw_width gmw_height gmw_pixels gmw_push gmw_cursor gmw_title gmw_clip_get gmw_clip_set gmw_alloc gmw_free)
"$ld" --no-entry --strip-all --gc-sections -z stack-size=1048576 --initial-memory=4194304 "${exports[@]/#/--export=}" -o "$out/goome.wasm" "${objs[@]}"

title="$(grep -ohE '\.title[[:space:]]*=[[:space:]]*"[^"]*"|goome_open_app\([[:space:]]*"[^"]*"' <<<"$code" | head -1 | sed -E 's/^[^"]*"//; s/"$//' || true)"
[ -n "$title" ] || title="$(basename "${app%.c}")"
title="${title//&/&amp;}"
title="${title//</&lt;}"
sed "s|@TITLE@|${title//|/\\|}|" "$here/web/index.html" > "$out/index.html"
cp "$here/web/goome.js" "$out/goome.js"
echo "built $out/index.html, goome.js and goome.wasm ($(wc -c < "$out/goome.wasm") bytes)"
