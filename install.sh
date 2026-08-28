set -e

echo ""
echo "=== CHROMELAB INSTALLER ==="
echo ""

if [ "$(id -u)" -ne 0 ]; then
    echo "error: must be run as root"
    exit 1
fi

HOME_DIR="/home/lab"
REPO_DIR="${HOME_DIR}/chromelab"
BUILD_DIR="${REPO_DIR}/build"
PREFIX="/usr/local"
CONF_DIR="/etc/chromelab"
DATA_DIR="/var/lib/chromelab"
LOG_DIR="/var/log/chromelab"
RUN_DIR="/run/chromelab"
WEB_DIR="${PREFIX}/share/chromelab/web"
BIN_DIR="${PREFIX}/bin"

REPO_URL="https://github.com/Rohan-Bharatia/chromelab.git"

echo "Installing base..."
apk update

if grep -q '^#.*\/community$' /etc/apk/repositories; then
    sed -i 's|^#\(.*\/community\)$|\1|' /etc/apk/repositories
fi
apk update

setup-xorg-base

if ! id lab >/dev/null 2>&1; then
    adduser -D lab
fi
if ! groups lab | grep -qw wheel; then
    addgroup lab wheel
fi
install -d -o lab -g lab -m 0755 "${HOME_DIR}"

apk add doas fastfetch

cat /etc/doas.conf <<"EOF"
permit persist :wheel
EOF
chmod 0400 /etc/doas.conf
chown root:root /etc/doas.conf

cat >"${HOME_DIR}/.xinitrc" <<'EOF'
xset r rate 200 35 &
xrandr -s 1920x1080
EOF
chmod 0644 "${HOME_DIR}/.xinitrc"
chown lab:lab "${HOME_DIR}/.xinitrc"

echo "Installing dependencies..."
apk add build-base \
    cmake \
    protobuf-dev \
    protobuf \
    grpc-dev \
    grpc \
    grpc-plugins \
    ncurses-dev \
    libmicrohttpd-dev \
    wireguard-tools \
    tailscale \
    git

ARCH=$(uname -m)
echo "Architecture: ${ARCH}"

if [ -d "${REPO_DIR}/.git" ]; then
    echo "Updating repository..."
    git -C "${REPO_DIR}" pull --ff-only --recurse-submodules
else
    echo "Cloning repository..."
    git clone "${REPO_URL}" "${REPO_DIR}" --recurse-submodules
fi
chown -R lab:lab "${REPO_DIR}"

echo "Creating directories..."
install -d -o root -g root -m 0755 "${BIN_DIR}"
install -d -o root -g root -m 0755 "${CONF_DIR}"
install -d -o root -g root -m 0755 "${DATA_DIR}/models"
install -d -o root -g root -m 0755 "${DATA_DIR}/events"
install -d -o root -g root -m 0755 "${LOG_DIR}"
install -d -o root -g root -m 0755 "${RUN_DIR}"
install -d -o root -g root -m 0755 "${WEB_DIR}"
install -d -o lab -g lab -m 0755 "${BUILD_DIR}"

echo "Building..."
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="Release"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 4)"

echo "Installing binaries..."
install -m 0755 "${BUILD_DIR}/labd" "${BIN_DIR}/labd"
install -m 0755 "${BUILD_DIR}/labctl" "${BIN_DIR}/labctl"

if [ ! -f "${CONF_DIR}/labd.conf" ]; then
    echo "Installing config..."
    install -m 0644 "${REPO_DIR}/etc/labd.conf" "${CONF_DIR}/labd.conf"
else
    echo "Config exists, skipping (etc/labd.conf -> ${CONF_DIR}/labd.conf)"
fi

echo "Installing web dashboard..."
cp -r "${REPO_DIR}/web/." "${WEB_DIR}/"

echo "Installing models..."
cp -r "${REPO_DIR}/models/." "${DATA_DIR}/models/"

echo "Installing init script..."
install -m 0755 "${REPO_DIR}/etc/init.d/labd" /etc/init.d/labd

echo "Enabling daemon..."
rc-update add labd default

echo "Starting daemon..."
rc-service labd start

echo ""
echo "=== INSTALLATION COMPLETE ==="
echo ""

echo "Quick start:"
echo "  1. Edit config:    vi ${CONF_DIR}/labd.conf"
echo "  2. Check daemon:   rc-service labd status"
echo "  3. Open dashboard: http://$(hostname -i | awk '{print $1}'):8080"
echo "  4. CLI usage:      labctl status"
echo ""
echo "Reboot when ready."
echo ""
