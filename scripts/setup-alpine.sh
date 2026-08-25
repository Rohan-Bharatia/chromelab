#!/usr/bin/env sh

su -

apk update
apk upgrade
apk add doas

# Allow community packages
sed -i 's|^#\(.*\/community\)$|\1|' /etc/apk/repositories
apk update

# Install all Xorg packages
setup-xorg-base

sh -c 'printf "%s\n" permit persist :wheel" > /etc/doas.conf'
adduser lab wheel

exit

# Add basic dependencies
doas apk add musl git vim fastfetch

sh -c 'printf "%s\n" "xset r rate 200 35 &" "xrandr -s 1920x1080" > "$HOME/.xinitrc"'
