#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "uninstall.sh must run as root" >&2
    exit 1
fi

rm -f /opt/cardputer-zero-shell/bin/zero-shell-wayland

if [ -d /opt/cardputer-zero-shell/bin ]; then
    rmdir /opt/cardputer-zero-shell/bin 2>/dev/null || true
fi
if [ -d /opt/cardputer-zero-shell ]; then
    rmdir /opt/cardputer-zero-shell 2>/dev/null || true
fi

echo "cardputer-zero-shell files removed"
