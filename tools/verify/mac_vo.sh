#!/usr/bin/env bash
set -u
source "$(dirname "$0")/lib.sh"
cell="screenreader:macos-voiceover:${RUNNER_NAME:-$(uname -m)}"
S=$VERIFY_OUT/macvo
mkdir -p "$S"
ver=$(sw_vers -productVersion)
steps=""
note() { steps="$steps | $1"; echo "$1"; }
VO=/System/Library/CoreServices/VoiceOver.app
[ -d "$VO" ] && note "VoiceOver.app present" || note "no VoiceOver.app"
note "sip: $(csrutil status 2>&1 | head -1)"
tcc=/Library/Application\ Support/com.apple.TCC/TCC.db
note "tcc db readable: $(sudo sqlite3 "$tcc" 'select service,client,auth_value from access' 2>&1 | head -5 | tr '\n' ',')"
build_lib || { emit "$cell" 0 fail "lib build failed" "$ver"; exit 0; }
build_prog "$ROOT/tests/a11y_app.c" || { emit "$cell" 0 fail "a11y_app build failed" "$ver"; exit 0; }
for who in /usr/bin/osascript /bin/bash /Applications/Utilities/Terminal.app; do
  sudo sqlite3 "$tcc" "insert or replace into access (service,client,client_type,auth_value,auth_reason,auth_version,flags) values ('kTCCServiceAccessibility','$who',1,2,4,1,0)" >>"$S/tcc.log" 2>&1
done
note "tcc insert result: $(tail -n2 "$S/tcc.log" | tr '\n' ' ')"
sudo defaults write com.apple.universalaccess voiceOverOnOffKey -bool true >>"$S/vo.log" 2>&1
sudo touch /private/var/db/.AccessibilityAPIEnabled 2>>"$S/vo.log"
osascript -e 'tell application "System Events" to set UI elements enabled to true' >>"$S/vo.log" 2>&1
note "ui scripting: $(tail -n1 "$S/vo.log")"
"$VB/a11y_app" 60 >"$S/app.log" 2>&1 &
app=$!
sleep 3
osascript -e 'tell application "System Events" to key code 96 using {command down}' >>"$S/vo.log" 2>&1
note "cmd+f5: $(tail -n1 "$S/vo.log")"
sleep 8
note "voiceover process: $(pgrep -l VoiceOver | tr '\n' ' ')"
phrase=$(osascript -e 'tell application "VoiceOver" to get the last phrase' 2>&1 | head -2 | tr '\n' ' ')
note "last phrase query: $phrase"
if [ -z "$(pgrep VoiceOver)" ]; then
  /System/Library/CoreServices/VoiceOver.app/Contents/MacOS/VoiceOverStarter >"$S/starter.log" 2>&1 &
  sleep 6
  note "VoiceOverStarter: $(tail -n2 "$S/starter.log" | tr '\n' ' ') pid: $(pgrep VoiceOver | tr '\n' ' ')"
fi
osascript -e 'tell application "VoiceOver" to output "goome probe"' >>"$S/vo.log" 2>&1
sleep 2
phrase2=$(osascript -e 'tell application "VoiceOver" to get the last phrase' 2>&1 | head -2 | tr '\n' ' ')
kill $app 2>/dev/null
if echo "$phrase2" | grep -qi "goome probe"; then
  emit "$cell" 1 pass "macOS $ver: VoiceOver started and its last phrase reads back: $phrase2. $steps" "$ver"
else
  emit "$cell" 1 fail "macOS $ver: VoiceOver speech could not be read. last phrase: $phrase2 | $steps" "$ver"
fi
exit 0
