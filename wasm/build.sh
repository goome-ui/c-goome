#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
clang="${CLANG:-clang}"
if ! command -v "$clang" >/dev/null; then echo "no clang, set CLANG=" >&2; exit 1; fi
ld="${WASM_LD:-$(dirname "$(command -v "$clang")")/wasm-ld}"
[ -x "$ld" ] || ld="$(command -v wasm-ld || true)"
if [ -z "$ld" ]; then echo "no wasm-ld, install lld or set WASM_LD=" >&2; exit 1; fi
if ! "$clang" --print-targets 2>/dev/null | grep -q wasm32; then echo "$clang cant target wasm32" >&2; exit 1; fi
out=out
mkdir -p "$out/obj"
res="$("$clang" -print-resource-dir)"
flags=(--target=wasm32 -nostdlib -nostdlibinc -isystem libc/include -isystem "$res/include" -std=c99 -Os -ffunction-sections -fdata-sections
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function ${WERROR:+-Werror} -I../include -I../src -I.)
objs=()
for f in ../src/goome.c ../src/draw.c ../src/image.c ../src/font.c ../src/ui.c ../src/test.c ../src/offscreen.c libc/libc.c gw.c; do
  o="$out/obj/$(basename "${f%.c}").o"
  "$clang" "${flags[@]}" -c "$f" -o "$o"
  objs+=("$o")
done
exports=(gw_init gw_resize gw_font_mem gw_load gw_error gw_push gw_frame gw_width gw_height gw_pixels gw_hash gw_cursor gw_log_take gw_clip_get gw_clip_set gw_alloc gw_free)
"$ld" --no-entry --strip-all --gc-sections -z stack-size=1048576 --initial-memory=4194304 "${exports[@]/#/--export=}" -o "$out/goome.wasm" "${objs[@]}"
cc="${CC:-cc}"
"$cc" -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers ${WERROR:+-Werror} -I../include -I../src -I. -o "$out/gw-native" native.c gw.c ../src/*.c
echo "built $out/goome.wasm ($(wc -c < "$out/goome.wasm") bytes) and $out/gw-native"
if [ -n "${1:-}" ]; then cp "$out/goome.wasm" "$1/goome.wasm" && echo "copied to $1/goome.wasm"; fi
