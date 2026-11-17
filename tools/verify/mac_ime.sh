#!/usr/bin/env bash
set -u
source "$(dirname "$0")/lib.sh"
ver=$(sw_vers -productVersion)
S=$VERIFY_OUT/macime-$(uname -m)
mkdir -p "$S"
cell="ime:macos:$(uname -m)"
build_lib || { emit "$cell" 0 fail "lib build failed" "$ver"; exit 0; }
build_prog "$ROOT/tests/ime_ui.c" || { emit "$cell" 0 fail "ime_ui build failed" "$ver"; exit 0; }
cat >"$S/tis.swift" <<'EOS'
import Carbon
import Foundation
let id = CommandLine.arguments[1]
let all = TISCreateInputSourceList([kTISPropertyInputSourceID as String: id] as CFDictionary, true).takeRetainedValue() as! [TISInputSource]
print("matches for \(id): \(all.count)")
if let s = all.first {
    print("enable \(TISEnableInputSource(s))")
    print("select \(TISSelectInputSource(s))")
}
if let cur = TISCopyCurrentKeyboardInputSource()?.takeRetainedValue(), let p = TISGetInputSourceProperty(cur, kTISPropertyInputSourceID) {
    print("current " + (Unmanaged<CFString>.fromOpaque(p).takeUnretainedValue() as String))
}
EOS
cat >"$S/keys.swift" <<'EOS'
import Cocoa
import Foundation
print("ax trusted \(AXIsProcessTrusted())")
let map: [Character: CGKeyCode] = ["n": 45, "i": 34, "h": 4, "a": 0, "o": 31, "g": 5, " ": 49]
for c in CommandLine.arguments[1] {
    guard let k = map[c] else { continue }
    for down in [true, false] {
        CGEvent(keyboardEventSource: nil, virtualKey: k, keyDown: down)?.post(tap: .cghidEventTap)
        usleep(30000)
    }
    usleep(250000)
}
EOS
run_one() {
  local tag=$1 src=$2 keys=$3 want=$4
  local c="$cell:$tag"
  local si
  si=$(swiftc -O -o "$S/tis" "$S/tis.swift" 2>&1 | tail -n3 | tr '\n' ' ')
  swiftc -O -o "$S/keys" "$S/keys.swift" >"$S/keys-build.log" 2>&1
  local tis
  tis=$("$S/tis" "$src" 2>&1 | tr '\n' ';')
  "$VB/ime_ui" >"$S/ime-$tag.log" 2>&1 &
  local app=$!
  sleep 4
  osascript -e 'tell application "System Events" to set frontmost of first process whose unix id is '"$app"' to true' >>"$S/osa.log" 2>&1
  sleep 1
  local k
  k=$("$S/keys" "$keys" 2>&1 | tr '\n' ';')
  sleep 1.5
  "$S/keys" " " >/dev/null 2>&1
  sleep 1.5
  osascript -e 'tell application "System Events" to key code 36' >>"$S/osa.log" 2>&1
  sleep 1.5
  kill $app 2>/dev/null
  local lines
  lines=$(tr '\n' ';' <"$S/ime-$tag.log" | cut -c1-600)
  local det="macOS $ver $(uname -m); swiftc: $si; tis: $tis; keys: $k; osa: $(tail -n1 "$S/osa.log"); goome log: $lines"
  if grep -E '^(text|buf) ' "$S/ime-$tag.log" | grep -qE "$want"; then emit "$c" 1 pass "$det" "$ver"; else emit "$c" 1 fail "$det" "$ver"; fi
}
run_one pinyin com.apple.inputmethod.SCIM.ITABC nihao "你好"
run_one kotoeri com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese nihongo "にほんご|日本語"
exit 0
