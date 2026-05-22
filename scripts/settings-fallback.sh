#!/bin/sh
set -eu

while :; do
    clear
    cat <<'EOF'
ZeroShell Settings

1. nmtui
2. restart networking
3. back
EOF
    printf '\nSelect: '
    IFS= read -r choice || exit 0
    case "$choice" in
        1) nmtui ;;
        2) sudo /usr/local/sbin/zero-helper network-restart ;;
        3|q|Q) exit 0 ;;
    esac
done
