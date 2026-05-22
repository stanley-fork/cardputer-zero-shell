# APPLaunch Application Contract

本文档定义 ZeroShell 兼容的 APPLaunch 应用发现契约。

## Directory

Production scan directory:

```text
/usr/share/APPLaunch/applications
```

ZeroShell scans:

```text
/usr/share/APPLaunch/applications/*.desktop
```

ZeroShell intentionally does not scan:

```text
/usr/share/applications
```

Reason:

普通 Linux 桌面应用太多，直接混入 Cardputer Zero 小屏界面会破坏设备体验。ZeroShell 的应用入口应由 APPLaunch-compatible `.desktop` 明确声明。

## Data Directories

MVP keeps the APPLaunch path convention:

```text
/usr/share/APPLaunch
/usr/share/APPLaunch/applications
/usr/share/APPLaunch/share/images
/usr/share/APPLaunch/share/font
/usr/share/APPLaunch/share/audio
```

This is a compatibility surface. It does not mean ZeroShell owns the whole APPLaunch ecosystem.

## Desktop Entry Subset

Supported format:

```ini
[Desktop Entry]
Name=Terminal
TryExec=bash
Exec=bash
Terminal=true
Icon=share/images/cli_100.png
Sysplause=false
```

Supported fields:

| Field | Required | Meaning |
| --- | --- | --- |
| `Name` | yes | Display name |
| `Exec` | yes | Command to run |
| `Icon` | no | APPLaunch-compatible icon path |
| `Terminal` | no | Run in ZeroShell terminal page when true |
| `TryExec` | no | Hide app when executable is unavailable |
| `Sysplause` | no | Pause after terminal command exits when true |

Unknown fields are ignored.

## Name

`Name` is the label displayed in the launcher.

Example:

```ini
Name=Terminal
```

MVP UI may truncate long labels to fit the 320x170 screen.

## Exec

`Exec` is the command ZeroShell runs.

Example:

```ini
Exec=bash
```

The MVP runs commands through `/bin/sh -lc` for blocking external commands and through a PTY shell for terminal commands.

## TryExec

`TryExec` is used to decide whether the app should be shown.

Examples:

```ini
TryExec=bash
TryExec=/opt/cardputer-zero-shell/scripts/files-fallback.sh
```

If `TryExec` is missing, ZeroShell uses the first token of `Exec`.

If the command is unavailable, the entry is hidden.

## Terminal

`Terminal=true` means:

```text
open internal ZeroShell PTY terminal page
run Exec inside that terminal
```

Good examples:

```ini
Exec=bash
Terminal=true
```

```ini
Exec=top
Terminal=true
```

`Terminal=false` means:

```text
run Exec as a blocking external process
return to ZeroShell when it exits
```

## Sysplause

`Sysplause=true` means ZeroShell should pause after a terminal command exits so the user can read the result.

`Sysplause=false` means return without the extra pause.

The name is preserved for compatibility with the existing APPLaunch convention.

## Icon

`Icon` can be an APPLaunch relative path:

```ini
Icon=share/images/cli_100.png
```

or an absolute path:

```ini
Icon=/usr/share/APPLaunch/share/images/cli_100.png
```

The current framebuffer MVP renders icon tiles using app initials. The icon field is still parsed and preserved as part of the compatibility contract, so a future LVGL/image backend can render it.

## Duplicate Handling

If multiple `.desktop` files have the same `Exec`, the first loaded entry wins and later duplicates are skipped.

Files are loaded in sorted path order.

## Installing An App

Install a new app by placing a `.desktop` file in:

```text
/usr/share/APPLaunch/applications
```

Example:

```sh
sudo install -m 0644 my-tool.desktop /usr/share/APPLaunch/applications/my-tool.desktop
```

Then press `R` in ZeroShell to reload.

Debian packages can install APPLaunch entries through their normal package
layout. For example, a Cardputer-specific package may install:

```text
/usr/share/APPLaunch/applications/my-tool.desktop
/usr/share/APPLaunch/share/images/my-tool.png
```

ZeroShell watches the APPLaunch directory metadata as well as entry mtimes, so
package installs can be picked up without relying on packaged file timestamps.

## Fixed Tools

Tools such as Settings, Files, Terminal, App Store, System Monitor and HDMI should be represented by `.desktop` files.

They should not be hard-coded as C++ built-in apps.
