#!/bin/sh
set -eu
v="${1:?usage: packaging/bump.sh 0.4.0}"
v="${v#v}"
here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d "$here/.bump.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

curl -fsSL "https://github.com/goome-ui/c-goome/archive/refs/tags/v$v.tar.gz" -o "$tmp/c.tgz"
s512=$(sha512sum "$tmp/c.tgz" | cut -d' ' -f1)
s256=$(sha256sum "$tmp/c.tgz" | cut -d' ' -f1)

sed -i "s/\"version\": \"[^\"]*\"/\"version\": \"$v\"/" "$here/vcpkg/ports/goome/vcpkg.json"
sed -i "s/SHA512 [0-9a-f]*/SHA512 $s512/" "$here/vcpkg/ports/goome/portfile.cmake"

if ! grep -q "\"$v\":" "$here/conan/config.yml"; then
  printf '  "%s":\n    folder: all\n' "$v" >> "$here/conan/config.yml"
  printf '  "%s":\n    url: "https://github.com/goome-ui/c-goome/archive/refs/tags/v%s.tar.gz"\n    sha256: "%s"\n' "$v" "$v" "$s256" >> "$here/conan/all/conandata.yml"
fi

aur() {
  sed -i "s/^pkgver=.*/pkgver=$1/; s/^pkgrel=.*/pkgrel=1/; s/^sha256sums=.*/sha256sums=('$2')/" "$3/PKGBUILD"
  sed -i "s/pkgver = .*/pkgver = $1/; s/pkgrel = .*/pkgrel = 1/; s/sha256sums = .*/sha256sums = $2/; s/-[0-9][0-9.]*\.tar\.gz::/-$1.tar.gz::/; s|/v[0-9][0-9.]*\.tar\.gz|/v$1.tar.gz|" "$3/.SRCINFO"
}
aur "$v" "$s256" "$here/aur/goome"

if curl -fsSL "https://github.com/goome-ui/goome-cli/archive/refs/tags/v$v.tar.gz" -o "$tmp/cli.tgz" 2>/dev/null; then
  aur "$v" "$(sha256sum "$tmp/cli.tgz" | cut -d' ' -f1)" "$here/aur/goome-cli"
else
  echo "goome-cli has no v$v tag yet, left aur/goome-cli alone"
fi

if [ -n "${GOOME_TAP:-}" ] && [ -f "$GOOME_TAP/Formula/goome.rb" ]; then
  sed -i "s|archive/refs/tags/v[0-9][0-9.]*\.tar\.gz|archive/refs/tags/v$v.tar.gz|; s|^  sha256 .*|  sha256 \"$s256\"|" "$GOOME_TAP/Formula/goome.rb"
  echo "formula in $GOOME_TAP is at $v, commit and push it"
fi

echo "packaging is at $v"
