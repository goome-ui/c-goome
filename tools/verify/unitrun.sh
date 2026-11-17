#!/usr/bin/env bash
set -u
source "$(dirname "$0")/lib.sh"
cell="unit:${RUNNER_OS:-$(uname -s)}-$(uname -m)"
out=$("$@" 2>&1)
rc=$?
echo "$out" | tail -n 5
n=$(echo "$out" | grep -oE '^CHECKS [0-9]+' | tail -n1 | cut -d' ' -f2)
img="${ImageOS:-?} ${ImageVersion:-?}"
if [ $rc = 0 ] && echo "$out" | grep -q '^all good' && [ -n "$n" ]; then
  emit "$cell" 1 pass "tests/unit.c ran $n CHECK evaluations, all good, runner image $img" "$img"
else
  emit "$cell" 1 fail "rc $rc, checks $n, runner image $img: $(echo "$out" | grep -E 'FAIL|failed' | head -n 5 | tr '\n' ';')" "$img"
fi
