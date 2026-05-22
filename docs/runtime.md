# Runtime Contract

本文档定义 ZeroShell 的运行身份、启动路径、环境变量和系统依赖。

## Launch Chain

The intended launch chain is:

```text
zero-greeter
  -> PAM authenticates existing system user
  -> cardputer-zero-session
  -> /opt/cardputer-zero-shell/bin/zero-shell
```

`zero-greeter` and PAM are not part of ZeroShell. They belong to `cardputer-zero-os`.

## Binary Path

Canonical installed binary:

```text
/opt/cardputer-zero-shell/bin/zero-shell
```

`cardputer-zero-session` should exec this binary after login.

## User Identity

ZeroShell must run as the authenticated Linux user.

Check:

```sh
ps -eo user,pid,args | grep zero-shell
```

Expected:

```text
pi      1234 /opt/cardputer-zero-shell/bin/zero-shell
```

Not acceptable:

```text
root    1234 /opt/cardputer-zero-shell/bin/zero-shell
```

The current implementation refuses to run when `geteuid() == 0`.

## Required Permissions

ZeroShell needs user-level access to:

- the internal framebuffer device, normally `/dev/fb0` or another `fb_st7789` framebuffer,
- the Cardputer keyboard evdev device,
- user runtime environment for child applications.

Device permission setup belongs to `cardputer-zero-os`.

ZeroShell should not try to fix permissions by running itself as root.

## Display Target

ZeroShell targets the internal Cardputer Zero screen.

Framebuffer selection:

1. `ZEROSHELL_FBDEV` if set,
2. `/proc/fb` entry whose name contains `st7789`,
3. `/sys/class/graphics/fb*/name` containing `st7789`,
4. fallback `/dev/fb0`.

This is intentionally separate from HDMI and desktop display managers.

## Input Target

Keyboard selection:

1. `ZEROSHELL_KEYBOARD_DEVICE` if set,
2. `/dev/input/by-path/*3f804000.i2c*event*`,
3. `/sys/class/input/event*/device/name` containing `keyboard`, `keypad`, or `tca8418`,
4. fallback `/dev/input/event0`.

## Environment Variables

Supported environment variables:

| Variable | Meaning |
| --- | --- |
| `ZEROSHELL_FBDEV` | Override framebuffer device |
| `ZEROSHELL_KEYBOARD_DEVICE` | Override keyboard evdev device |
| `ZEROSHELL_APPLICATIONS_DIR` | Override application scan directory for development |
| `ZEROSHELL_APPLAUNCH_DIR` | Override APPLaunch data root for development |

Production default:

```text
ZEROSHELL_APPLICATIONS_DIR=/usr/share/APPLaunch/applications
ZEROSHELL_APPLAUNCH_DIR=/usr/share/APPLaunch
```

## Child Process Identity

Applications launched by ZeroShell inherit the same user identity as ZeroShell.

ZeroShell must not launch user applications as root.

If an application requires privileged actions, that app should use the restricted helper contract provided by `cardputer-zero-os`, usually:

```text
sudo /usr/local/sbin/zero-helper <allowed-action>
```

## Session Recovery

If `/opt/cardputer-zero-shell/bin/zero-shell` is missing, recovery belongs to `cardputer-zero-os` via `cardputer-zero-session`.

ZeroShell itself should not own login recovery.

## Development Runtime

For development, run as a normal user:

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell
```

Do not use `sudo` for normal shell execution.

