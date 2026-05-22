#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "uninstall.sh must run as root" >&2
    exit 1
fi

rm -f /opt/cardputer-zero-shell/bin/zero-shell
rm -f /opt/cardputer-zero-shell/scripts/files-fallback.sh
rm -f /opt/cardputer-zero-shell/scripts/settings-fallback.sh
rm -f /opt/cardputer-zero-shell/scripts/monitor-fallback.sh
rm -f /opt/cardputer-zero-shell/scripts/app-store-fallback.sh
rm -f /opt/cardputer-zero-shell/scripts/hdmi-menu.sh

rm -f /usr/share/APPLaunch/applications/terminal.desktop
rm -f /usr/share/APPLaunch/applications/files.desktop
rm -f /usr/share/APPLaunch/applications/settings.desktop
rm -f /usr/share/APPLaunch/applications/system-monitor.desktop
rm -f /usr/share/APPLaunch/applications/app-store.desktop
rm -f /usr/share/APPLaunch/applications/hdmi.desktop

echo "cardputer-zero-shell files removed"

