#!/usr/bin/env bash
# ./build.sh          lib + examples + tests into build/
# ./build.sh test     same, then runs the tests
# ./build.sh win      windows exes with mingw, into build-win/
# ./build.sh single   remakes the one file goome.h
set -euo pipefail
cd "$(dirname "$0")"
what="${1:-}"
py=$(command -v python3 || command -v python || true)
if [ -n "$py" ]; then "$py" tools/single.py >/dev/null; fi
warn=(-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers ${WERROR:+-Werror} -Iinclude -Isrc)
srcs=(src/*.c)
case "$what" in
single) exit 0 ;;
win)
  cc="${CC:-x86_64-w64-mingw32-gcc}"
  out=build-win
  ext=.exe
  ;;
*)
  cc="${CC:-cc}"
  out=build
  ext=
  ;;
esac
mkdir -p "$out/obj"
objs=()
for f in "${srcs[@]}"; do
  o="$out/obj/$(basename "${f%.c}").o"
  "$cc" -O2 "${warn[@]}" -c "$f" -o "$o"
  objs+=("$o")
done
rm -f "$out/libgoome.a"
"${AR:-${cc%gcc}ar}" rcs "$out/libgoome.a" "${objs[@]}" 2>/dev/null || ar rcs "$out/libgoome.a" "${objs[@]}"
for f in examples/*.c tests/unit.c; do
  "$cc" -O2 "${warn[@]}" -o "$out/$(basename "${f%.c}")$ext" "$f" "$out/libgoome.a"
done
printf '#define GOOME_IMPLEMENTATION\n#include "goome.h"\n' > "$out/one.c"
"$cc" -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers ${WERROR:+-Werror} -I. -o "$out/one$ext" examples/hello.c "$out/one.c"
echo "built $out/libgoome.a, examples and tests"
if [ "$what" = test ]; then "$out/unit"; fi
