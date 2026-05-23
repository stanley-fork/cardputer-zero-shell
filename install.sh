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
    CC=${CC:-cc}
    if ! command -v "$CXX" >/dev/null 2>&1; then
        echo "install.sh needs cmake or a C++ compiler" >&2
        exit 1
    fi
    if ! command -v wayland-scanner >/dev/null 2>&1; then
        echo "install.sh needs wayland-scanner for zero-shell-wayland" >&2
        echo "Install wayland-protocols and libwayland-dev, or use cmake." >&2
        exit 1
    fi

    XDG_SHELL_XML=${XDG_SHELL_XML:-}
    if [ -z "$XDG_SHELL_XML" ]; then
        for candidate in \
            /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
            /usr/share/wayland/xdg-shell.xml; do
            if [ -r "$candidate" ]; then
                XDG_SHELL_XML=$candidate
                break
            fi
        done
    fi
    if [ -z "$XDG_SHELL_XML" ]; then
        echo "install.sh needs xdg-shell.xml for zero-shell-wayland" >&2
        echo "Install wayland-protocols." >&2
        exit 1
    fi

    mkdir -p "$BUILD_DIR"
    wayland-scanner client-header "$XDG_SHELL_XML" "$BUILD_DIR/xdg-shell-client-protocol.h"
    wayland-scanner private-code "$XDG_SHELL_XML" "$BUILD_DIR/xdg-shell-protocol.c"
    "$CC" -c "$BUILD_DIR/xdg-shell-protocol.c" \
        $(pkg-config --cflags wayland-client 2>/dev/null || true) \
        -o "$BUILD_DIR/xdg-shell-protocol.o"
    "$CXX" -std=c++17 -Wall -Wextra -Wpedantic \
        -I"$ROOT_DIR/main/include" \
        -I"$BUILD_DIR" \
        "$ROOT_DIR/main/src/app_catalog.cpp" \
        "$ROOT_DIR/main/src/image.cpp" \
        "$ROOT_DIR/main/src/status.cpp" \
        "$ROOT_DIR/main/src/zero_shell_wayland.cpp" \
        "$BUILD_DIR/xdg-shell-protocol.o" \
        $(pkg-config --cflags --libs wayland-client libpng 2>/dev/null || printf '%s' '-lwayland-client -lpng -lm') \
        -o "$BUILD_DIR/zero-shell-wayland"
fi

install -d -m 0755 /opt/cardputer-zero-shell/bin
install -d -m 0755 /usr/share/APPLaunch/applications
install -d -m 0755 /usr/share/APPLaunch/share/images

install -m 0755 "$BUILD_DIR/zero-shell-wayland" /opt/cardputer-zero-shell/bin/zero-shell-wayland

echo "cardputer-zero-shell installed:"
echo "  /opt/cardputer-zero-shell/bin/zero-shell-wayland"
echo "  /usr/share/APPLaunch/applications"
