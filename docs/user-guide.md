# User Guide

本文档描述最终用户如何使用 ZeroShell。

## Where ZeroShell Appears

ZeroShell 出现在用户登录成功之后。

正常流程：

```text
Raspberry Pi OS boots
  -> cardputer-zero-os greeter
  -> user logs in
  -> cardputer-zero-session
  -> ZeroShell home screen
```

ZeroShell 不显示在：

- firmware boot stage
- kernel boot stage
- GUI greeter before login
- HDMI LightDM login

## Home Screen

MVP home screen 是一个五槽位 carousel：

```text
left outer / left / center / right / right outer
```

中心槽位是当前选中的应用。按 `Enter` 会启动中心应用。

顶部状态栏显示：

- `ZeroShell`
- WiFi 状态
- battery 百分比
- time

底部显示基础操作提示。

## Controls

MVP controls:

| Key | Action |
| --- | --- |
| `Left` | Select previous app |
| `Right` | Select next app |
| `Enter` | Open selected app |
| `R` | Reload `/usr/share/APPLaunch/applications` |
| `Esc` | Open power menu |
| `Q` | Quit shell, mainly for development/recovery |

## Launching Apps

ZeroShell launches apps from:

```text
/usr/share/APPLaunch/applications/*.desktop
```

Each `.desktop` file declares the app name, command, icon and launch mode.

Example:

```ini
[Desktop Entry]
Name=Terminal
TryExec=bash
Exec=bash
Terminal=true
Icon=share/images/cli_100.png
Sysplause=false
```

ZeroShell also watches the APPLaunch directory and reloads when package installs
or manual file changes update the directory metadata. Press `R` if you want to
force a reload immediately.

## Terminal Apps

If an entry has:

```ini
Terminal=true
```

ZeroShell opens an internal framebuffer PTY terminal page and runs the command there.

Examples:

- `bash`
- `top`
- `htop`
- `nmtui`
- small command-line tools

The MVP terminal is intentionally minimal. It is enough for basic interaction, but it is not yet a full terminal emulator for every advanced curses application.

## External Apps

If an entry has:

```ini
Terminal=false
```

ZeroShell runs the command as a blocking external app. When the command exits, ZeroShell returns to the home screen.

This mode is intended for applications that can take over the screen or run their own UI.

## Reloading Apps

Press:

```text
R
```

to rescan:

```text
/usr/share/APPLaunch/applications
```

ZeroShell also watches directory modification time during its main loop and reloads when `.desktop` files change.

## Power Menu

Press:

```text
Esc
```

to open the power menu.

MVP menu items:

- Cancel
- Reboot
- Shutdown

Privileged power actions are routed through:

```text
sudo /usr/local/sbin/zero-helper
```

ZeroShell does not directly run `sudo reboot`, `sudo shutdown`, arbitrary `systemctl`, or arbitrary shell commands for system actions.

## HDMI And Other Screens

ZeroShell is for the Cardputer Zero internal small screen.

It is not the HDMI login path. HDMI can continue to use the base OS display manager, such as LightDM, as configured by `cardputer-zero-os` and Raspberry Pi OS.

ZeroShell should not disable or replace HDMI login.
