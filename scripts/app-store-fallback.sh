#!/bin/sh
set -eu

clear
cat <<'EOF'
ZeroShell App Store

No standalone app-store UI is installed yet.

Install APPLaunch-compatible tools by placing .desktop files in:

  /usr/share/APPLaunch/applications

Press Enter to return.
EOF
IFS= read -r _

