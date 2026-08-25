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

# Install OXWM (DWM but better)
doas apk add zig libx11-dev libxft-dev libxinerama-dev freetype fontconfig git
git clone https://github.com/tonybanters/oxwm
cd oxwm
zig build
doas cp zig-out/bin/oxwm /usr/bin/oxwm
cd
rm -rf oxwm
sh -c 'printf "%s\n" "xset r rate 200 35 &" "exec oxwm;" > "$HOME/.xinitrc"'

# Install xf86-video-amdgpu if on an AMD system
doas apk add musl xf86-video-intel \
    font-terminus-nerd \
    vim dmenu st dillo xwallpaper \
    fastfetch
