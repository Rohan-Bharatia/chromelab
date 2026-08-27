#!/bin/sh
# chromelab installer for Alpine Linux (Acer Chromebook Spin 11)
# Run as root: sh install.sh

set -e

PREFIX="/usr/local"
CONFDIR="/etc/chromelab"
DATADIR="/var/lib/chromelab"
LOGDIR="/var/log/chromelab"
RUNDIR="/run/chromelab"
WEBDIR="${PREFIX}/share/chromelab/web"
BINDIR="${PREFIX}/bin"

echo "=== chromelab installer ==="

# Check root
if [ "$(id -u)" -ne 0 ]; then
    echo "error: must be run as root"
    exit 1
fi

# Detect architecture
ARCH=$(uname -m)
echo "Architecture: ${ARCH}"

# Check for required binaries
for bin in labd labctl; do
    if [ ! -f "./build/${bin}" ]; then
        echo "error: ./build/${bin} not found"
        echo "Run 'cmake --build build' first."
        exit 1
    fi
done

# Create directory structure
echo "Creating directories..."
install -d -o root -g root -m 0755 "${BINDIR}"
install -d -o root -g root -m 0755 "${CONFDIR}"
install -d -o root -g root -m 0755 "${DATADIR}/models"
install -d -o root -g root -m 0755 "${DATADIR}/events"
install -d -o root -g root -m 0755 "${LOGDIR}"
install -d -o root -g root -m 0755 "${RUNDIR}"
install -d -o root -g root -m 0755 "${WEBDIR}"

# Install binaries
echo "Installing binaries..."
install -m 0755 ./build/labd "${BINDIR}/labd"
install -m 0755 ./build/labctl "${BINDIR}/labctl"

# Install config (don't overwrite existing)
if [ ! -f "${CONFDIR}/labd.conf" ]; then
    echo "Installing config..."
    install -m 0644 ./etc/labd.conf "${CONFDIR}/labd.conf"
else
    echo "Config exists, skipping (etc/labd.conf -> ${CONFDIR}/labd.conf)"
fi

# Install web dashboard
echo "Installing web dashboard..."
cp -r ./web/* "${WEBDIR}/"

# Install OpenRC init script
if [ -d /etc/init.d ]; then
    echo "Installing init script..."
    install -m 0755 ./etc/init.d/labd /etc/init.d/labd
    echo "  Enable on boot:  rc-update add labd default"
    echo "  Start now:       rc-service labd start"
fi

# Install WireGuard if not present
if ! command -v wg >/dev/null 2>&1; then
    echo "Installing WireGuard..."
    if command -v apk >/dev/null 2>&1; then
        apk add wireguard-tools
    else
        echo "  warning: could not install wireguard-tools (not apk-based?)"
    fi
fi

echo ""
echo "=== installation complete ==="
echo ""
echo "Quick start:"
echo "  1. Edit config:    vi ${CONFDIR}/labd.conf"
echo "  2. Start daemon:   labd -c ${CONFDIR}/labd.conf"
echo "  3. Open dashboard: http://$(hostname -I | awk '{print $1}'):8080"
echo "  4. CLI usage:      labctl status"
echo ""
echo "Enable as a service:"
echo "  rc-update add labd default"
echo "  rc-service labd start"
echo ""
