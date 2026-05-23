# Runtime Contract

This document defines how `cardputer-zero-shell` runs.

## Launch Chain

```text
cardputer-zero-os
  -> greetd/PAM authenticates an existing Linux user
  -> /usr/local/bin/cardputer-zero-session
  -> /usr/local/bin/cardputer-zero-labwc-session
  -> labwc on /dev/dri/cardputer-zero-internal
  -> /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

`zero-greeter-wayland`, greetd, PAM, labwc startup policy, input policy, polkit,
and DRM device selection belong to `cardputer-zero-os`.

## Binary Path

Installed binary:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
```

## User Identity

ZeroShell must run as the authenticated Linux user:

```sh
ps -eo user,pid,args | grep zero-shell-wayland
```

Expected:

```text
pi  1234 /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

Not acceptable:

```text
root  1234 /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

## Display Target

Display model:

```text
zero-shell-wayland
  -> Wayland protocol
  -> labwc compositor
  -> /dev/dri/cardputer-zero-internal
  -> internal ST7789 display
```

Output ownership, focus, activation, minimize, close, and stacking belong to
labwc.

## Input Target

Inside the labwc session:

- ordinary keys go to the focused Wayland client,
- `Tab` is bound by labwc to `zero-shell-control tasks`,
- short/long `Esc` is handled by `zero-key-policy` from `cardputer-zero-os`.

ZeroShell handles keys while it is focused, and reacts to the command file
written by `zero-shell-control` when it is not focused.

## Environment Variables

Supported development overrides:

| Variable | Meaning |
| --- | --- |
| `ZEROSHELL_APPLICATIONS_DIR` | Override the APPLaunch desktop-entry directory. |
| `ZEROSHELL_APPLAUNCH_DIR` | Override the APPLaunch data root. |
| `ZERO_SHELL_WAYLAND_DEBUG` | Enable extra key/debug logging in the Wayland client. |

Production defaults:

```text
ZEROSHELL_APPLICATIONS_DIR=/usr/share/APPLaunch/applications
ZEROSHELL_APPLAUNCH_DIR=/usr/share/APPLaunch
```

## App Process Identity

Apps launched by ZeroShell inherit the same user identity as ZeroShell.

If an app needs privileged actions, it should go through the restricted helper
contract provided by `cardputer-zero-os`, usually:

```text
pkexec /usr/local/sbin/zero-helper <allowed-action>
```

ZeroShell does not wrap arbitrary `sudo`, `systemctl`, or package-manager
commands.

## Failure Behavior

If `zero-shell-wayland` is missing or exits, recovery belongs to
`cardputer-zero-os` through SSH, HDMI LightDM, or service logs. ZeroShell itself
does not own login recovery.

## Development Runtime

Run inside a Wayland session:

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell-wayland
```

Do not use `sudo` for normal shell execution.
