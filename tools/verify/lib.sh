ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
export VERIFY_OUT=${VERIFY_OUT:-$ROOT/verify-out}
mkdir -p "$VERIFY_OUT"
VB=${VB:-$ROOT/verify-build}
PY=$(command -v python3 || command -v python)
emit() { "$PY" -I "$ROOT/tools/verify/emit.py" "$@"; }
tailof() { tail -n "${2:-25}" "$1" 2>/dev/null | tr '\n' '|' | cut -c1-1500; }
waitfor() { local i; for i in $(seq "${3:-50}"); do grep -qE -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done; return 1; }
build_lib() {
  mkdir -p "$VB/obj"
  [ -f "$VB/libgoome.a" ] && return 0
  local f objs=()
  for f in "$ROOT"/src/*.c; do
    cc -std=c99 -O1 -g -w -I"$ROOT/include" -I"$ROOT/src" -c "$f" -o "$VB/obj/$(basename "${f%.c}").o" || return 1
    objs+=("$VB/obj/$(basename "${f%.c}").o")
  done
  ar rcs "$VB/libgoome.a" "${objs[@]}"
}
build_prog() {
  local src=$1 name=${2:-$(basename "${1%.c}")}
  cc -std=c99 -O1 -g -w -I"$ROOT/include" -o "$VB/$name" "$src" "$VB/libgoome.a" -ldl -lm
}
