#!/usr/bin/env bash
# Install or remove build dependencies for chromelab
set -euo pipefail

ACTION="${1:-install}"

# Alpine Linux packages
ALPINE_PKGS=(
    build-base
    cmake
    g++
    protobuf-dev
    protobuf
    grpc-dev
    grpc
    grpc-plugins
    ncurses-dev
    libmicrohttpd-dev
    llama.cpp
    curl
    git
)

# Detect package manager and install/remove
install_alpine() {
    echo "Installing build dependencies on Alpine Linux..."
    apk update
    if [ "$ACTION" = "remove" ]; then
        apk del "${ALPINE_PKGS[@]}" || true
        echo "Build dependencies removed."
    else
        apk add "${ALPINE_PKGS[@]}"
        echo "Build dependencies installed."
    fi
}

install_nix() {
    echo "For NixOS, use a nix-shell or flake devShell."
    echo "Example:"
    echo "  nix-shell -p cmake gnumake gcc protobuf openssl grpc libmicrohttpd ncurses llama-cpp"
    if [ "$ACTION" = "remove" ]; then
        echo "Nothing to remove for Nix (dependencies are isolated in the store)."
    fi
}

install_arch() {
    echo "Installing build dependencies on Arch Linux..."
    if [ "$ACTION" = "remove" ]; then
        pacman -Rns cmake base-devel protobuf grpc libmicrohttpd ncurses llama-cpp 2>/dev/null || true
    else
        pacman -S --needed cmake base-devel protobuf grpc libmicrohttpd ncurses llama-cpp
    fi
}

install_debian() {
    echo "Installing build dependencies on Debian/Ubuntu..."
    if [ "$ACTION" = "remove" ]; then
        apt remove -y cmake g++ protobuf-compiler libprotobuf-dev libgrpc++-dev libmicrohttpd-dev libncurses-dev
    else
        apt update
        apt install -y cmake g++ protobuf-compiler libprotobuf-dev libgrpc++-dev libmicrohttpd-dev libncurses-dev
    fi

    # TODO: Install llama-cpp for Debian
}

if [ -f /etc/alpine-release ]; then
    install_alpine
elif command -v nix-env &>/dev/null && [ -f /etc/NIXOS ]; then
    install_nix
elif command -v pacman &>/dev/null; then
    install_arch
elif command -v apt &>/dev/null; then
    install_debian
else
    echo "ERROR: Unsupported system. Install these manually:"
    echo "  cmake g++ protobuf protobuf-dev grpc grpc-dev libmicrohttpd-dev ncurses-dev"
    exit 1
fi
