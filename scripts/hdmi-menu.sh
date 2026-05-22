#!/bin/sh
set -eu

while :; do
    clear
    cat <<'EOF'
ZeroShell Display

1. internal
2. mirror
3. extended
4. back
EOF
    printf '\nSelect: '
    IFS= read -r choice || exit 0
    case "$choice" in
        1) sudo /usr/local/sbin/zero-helper display internal ;;
        2) sudo /usr/local/sbin/zero-helper display mirror ;;
        3) sudo /usr/local/sbin/zero-helper display extended ;;
        4|q|Q) exit 0 ;;
    esac
done

