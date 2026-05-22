#!/bin/sh
set -eu

if command -v htop >/dev/null 2>&1; then
    exec htop
fi

exec top

