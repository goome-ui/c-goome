#!/usr/bin/env bash
set -u
kind=$1
name=$2
distro=${DISTRO:-ubuntu}
mkdir -p "${VERIFY_OUT:-verify-out}"
log=${VERIFY_OUT:-verify-out}/deps-$name-$distro.log
[ "$kind" = wl ] || log=${VERIFY_OUT:-verify-out}/deps-$name.log
: >"$log"
SUDO=
[ "$(id -u)" = 0 ] || SUDO=sudo
if [ "$distro" = arch ]; then
  inst() { $SUDO pacman -S --noconfirm --needed "$@"; }
  $SUDO pacman -Syu --noconfirm >>"$log" 2>&1
  base="base-devel git wayland wayland-protocols plasma-wayland-protocols xorg-server-xvfb xdotool xorg-xev xorg-xwayland xorg-xsetroot imagemagick dbus python python-gobject mesa libxkbcommon"
else
  export DEBIAN_FRONTEND=noninteractive
  $SUDO apt-get update -q >>"$log" 2>&1
  inst() { $SUDO apt-get install -y -q "$@"; }
  base="build-essential libwayland-dev wayland-protocols plasma-wayland-protocols xvfb xdotool x11-utils x11-apps x11-xserver-utils imagemagick xwayland dbus dbus-x11 python3-gi libgl1-mesa-dri"
fi
echo "== base: $base" >>"$log"
inst $base >>"$log" 2>&1 || echo "base install failed" >>"$log"
case $kind:$name in
wl:sway) extra="sway swaybg" ;;
wl:weston-*) extra="weston" ;;
wl:cage) extra="cage" ;;
wl:labwc) extra="labwc" ;;
wl:wayfire) extra="wayfire" ;;
wl:river) extra="river" ;;
wl:niri) extra="niri" ;;
wl:hyprland) extra="hyprland" ;;
wl:kwin) if [ "$distro" = arch ]; then extra="kwin"; else extra="kwin-wayland kwin-wayland-backend-virtual"; fi ;;
wl:mutter) extra="mutter pipewire" ;;
x11:none) extra="" ;;
x11:openbox) extra="openbox" ;;
x11:i3) extra="i3-wm" ;;
x11:xfwm4) extra="xfwm4" ;;
x11:xfwm4-picom) extra="xfwm4 picom" ;;
x11:metacity) extra="metacity" ;;
x11:marco) extra="marco libglib2.0-bin mate-settings-daemon-common mate-desktop-common" ;;
x11:mutter-x11) extra="mutter" ;;
x11:kwin-x11) extra="kwin-x11" ;;
x11:fluxbox) extra="fluxbox" ;;
x11:icewm) extra="icewm" ;;
orca:*) extra="orca at-spi2-core speech-dispatcher python3-gi gir1.2-atspi-2.0 openbox dconf-gsettings-backend gsettings-desktop-schemas" ;;
imex:ibus-libpinyin) extra="ibus ibus-libpinyin" ;;
imex:ibus-mozc) extra="ibus ibus-mozc" ;;
imex:ibus-anthy) extra="ibus ibus-anthy" ;;
imex:fcitx5-pinyin) extra="fcitx5 fcitx5-chinese-addons" ;;
imex:fcitx5-mozc) extra="fcitx5 fcitx5-mozc" ;;
imex:fcitx5-anthy) extra="fcitx5 fcitx5-anthy" ;;
imewl:fcitx5-pinyin) extra="sway wtype fcitx5 fcitx5-chinese-addons" ;;
imewl:fcitx5-mozc) extra="sway wtype fcitx5 fcitx5-mozc" ;;
imewl:ibus-libpinyin) extra="sway wtype ibus ibus-libpinyin" ;;
*) extra="" ;;
esac
echo "== extra: $extra" >>"$log"
if [ -n "$extra" ]; then
  if ! inst $extra >>"$log" 2>&1; then
    echo "EXTRA INSTALL FAILED" >>"$log"
    if [ "$distro" = arch ] && [ "$name" = river ]; then inst river-classic >>"$log" 2>&1; fi
  fi
fi
tail -n 5 "$log"
exit 0
