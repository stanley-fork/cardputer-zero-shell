#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "install.sh must run as root" >&2
    exit 1
fi

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"

if command -v cmake >/dev/null 2>&1; then
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR"
else
    CXX=${CXX:-c++}
    if ! command -v "$CXX" >/dev/null 2>&1; then
        echo "install.sh needs cmake or a C++ compiler" >&2
        exit 1
    fi

    mkdir -p "$BUILD_DIR"
    "$CXX" -std=c++17 -Wall -Wextra -Wpedantic \
        -I"$ROOT_DIR/main/include" \
        "$ROOT_DIR/main/src/app_catalog.cpp" \
        "$ROOT_DIR/main/src/framebuffer_canvas.cpp" \
        "$ROOT_DIR/main/src/input_device.cpp" \
        "$ROOT_DIR/main/src/main.cpp" \
        "$ROOT_DIR/main/src/process_runner.cpp" \
        "$ROOT_DIR/main/src/pty_terminal.cpp" \
        "$ROOT_DIR/main/src/shell_ui.cpp" \
        "$ROOT_DIR/main/src/status.cpp" \
        -lutil \
        -o "$BUILD_DIR/zero-shell"
fi

install -d -m 0755 /opt/cardputer-zero-shell/bin
install -d -m 0755 /opt/cardputer-zero-shell/scripts
install -d -m 0755 /usr/share/APPLaunch/applications
install -d -m 0755 /usr/share/APPLaunch/share/images

install -m 0755 "$BUILD_DIR/zero-shell" /opt/cardputer-zero-shell/bin/zero-shell

for script in "$ROOT_DIR"/scripts/*.sh; do
    [ -e "$script" ] || continue
    install -m 0755 "$script" "/opt/cardputer-zero-shell/scripts/$(basename "$script")"
done

for desktop in "$ROOT_DIR"/applications/*.desktop; do
    [ -e "$desktop" ] || continue
    install -m 0644 "$desktop" "/usr/share/APPLaunch/applications/$(basename "$desktop")"
done

if [ -d "$ROOT_DIR/main/assets/images" ]; then
    for image in "$ROOT_DIR"/main/assets/images/*; do
        [ -e "$image" ] || continue
        install -m 0644 "$image" "/usr/share/APPLaunch/share/images/$(basename "$image")"
    done
fi

echo "cardputer-zero-shell installed:"
echo "  /opt/cardputer-zero-shell/bin/zero-shell"
echo "  /usr/share/APPLaunch/applications"
