#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
out=build/macax
mkdir -p "$out"
cc -std=c99 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Iinclude -o "$out/a11y_app" tests/a11y_app.c build/libgoome.a || exit 1
cc -std=c99 -O1 -Wall -Wextra -Wno-unused-parameter -o "$out/ax_client" tests/ax_client.c || exit 1
"$out/ax_client" "$out/a11y_app" "$out/app.log"
rc=$?
if [ $rc = 3 ]; then
  echo "this runner wont let us be an ax client, checking the same nsaccessibility code from inside the app"
  GOOME_AX_SELF=1 "$out/a11y_app" 30 | tee "$out/self.log"
  grep -q "self ok" "$out/self.log" && grep -q "pressed 1" "$out/self.log" && grep -q "dark 1" "$out/self.log" && grep -q "volume 7.5" "$out/self.log" && grep -q "name Robin" "$out/self.log" && grep -q "bye fruit 2" "$out/self.log"
  rc=$?
  [ $rc = 0 ] && echo "macax self check ok (no ax client)" || echo "macax self check FAILED"
fi
exit $rc
