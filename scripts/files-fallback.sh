#!/bin/sh
set -eu

if command -v ranger >/dev/null 2>&1; then
    exec ranger "${HOME:-/}"
fi

if command -v mc >/dev/null 2>&1; then
    exec mc "${HOME:-/}"
fi

cd "${HOME:-/}"
while :; do
    clear
    printf 'ZeroShell Files\n\n'
    pwd
    printf '\n'
    ls -la
    printf '\nEnter path to cd/open, or q to quit: '
    IFS= read -r target || exit 0
    case "$target" in
        q|Q) exit 0 ;;
        "") continue ;;
    esac
    if [ -d "$target" ]; then
        cd "$target"
    elif [ -f "$target" ]; then
        "${PAGER:-less}" "$target" || true
    else
        printf 'Not found: %s\n' "$target"
        sleep 1
    fi
done

