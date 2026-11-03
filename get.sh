#!/bin/sh
# run it in your project folder:
#   curl -fsSL https://raw.githubusercontent.com/goome-ui/c-goome/main/get.sh | sh
set -eu

repo=goome-ui/c-goome
branch="${GOOME_BRANCH:-main}"
say() { printf '\033[1;32m>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!\033[0m %s\n' "$*" >&2; }

fetch() { # fetch <path in repo> <out>
  for b in "$branch" dev; do
    if command -v curl >/dev/null 2>&1; then
      curl -fsSL "https://raw.githubusercontent.com/$repo/$b/$1" -o "$2.part" 2>/dev/null && mv "$2.part" "$2" && return 0
    elif command -v wget >/dev/null 2>&1; then
      wget -qO "$2.part" "https://raw.githubusercontent.com/$repo/$b/$1" 2>/dev/null && mv "$2.part" "$2" && return 0
    fi
    if command -v gh >/dev/null 2>&1; then
      gh api "repos/$repo/contents/$1?ref=$b" -H "Accept: application/vnd.github.raw" > "$2.part" 2>/dev/null && mv "$2.part" "$2" && return 0
    fi
  done
  rm -f "$2.part"
  return 1
}

keep_old() {
  if [ -e "$1" ]; then
    cp "$1" "$1.old"
    warn "you already had $1, the old one is now $1.old"
  fi
}

find_cc() {
  for c in ${CC:-} cc gcc clang; do
    if [ -n "$c" ] && command -v "$c" >/dev/null 2>&1; then echo "$c"; return 0; fi
  done
  return 1
}

get_cc() {
  os=$(uname -s)
  if [ "$os" = Darwin ]; then
    warn "no c compiler yet, starting the xcode command line tools install. click Install in the popup, then run this again"
    xcode-select --install 2>/dev/null || true
    exit 1
  fi
  sudo=""
  [ "$(id -u)" -ne 0 ] && sudo=sudo
  if command -v apt-get >/dev/null 2>&1; then $sudo apt-get install -y gcc libc6-dev
  elif command -v dnf >/dev/null 2>&1; then $sudo dnf install -y gcc
  elif command -v pacman >/dev/null 2>&1; then $sudo pacman -S --needed --noconfirm gcc
  elif command -v zypper >/dev/null 2>&1; then $sudo zypper install -y gcc
  elif command -v apk >/dev/null 2>&1; then $sudo apk add gcc musl-dev
  elif command -v xbps-install >/dev/null 2>&1; then $sudo xbps-install -y gcc
  else
    warn "dont know your package manager, install gcc or clang yourself"
    exit 1
  fi
}

say "getting goome into $(pwd)"
keep_old goome.h
fetch goome.h goome.h || { warn "couldnt download goome.h (no internet, or the repo is still private and you have no gh login)"; exit 1; }
if [ ! -e hello.c ]; then
  fetch examples/hello.c hello.c || warn "couldnt get the example, goome.h is there though"
fi

cc=$(find_cc) || {
  say "you dont have a c compiler, getting one"
  get_cc
  cc=$(find_cc) || { warn "still no compiler, sorry"; exit 1; }
}
say "using $cc"

if [ -e hello.c ]; then
  [ -e goome.c ] || printf '#define GOOME_IMPLEMENTATION\n#include "goome.h"\n' > goome.c
  if "$cc" -O2 -o hello hello.c goome.c 2>goome-build.log; then
    rm -f goome-build.log
    say "built ./hello, run it to see a box you can click (alt+h hides it, esc quits)"
  else
    warn "the example didnt build, whats wrong is in goome-build.log"
    exit 1
  fi
fi

cat <<EOF

all set. in your own code:
  #include "goome.h"      everywhere you use it
  goome.c                 the one file that has the implementation, just compile it with the rest
build: $cc -O2 -o myapp myapp.c goome.c
EOF
