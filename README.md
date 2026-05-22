# cardputer-zero-shell

`cardputer-zero-shell` is the post-login GUI shell for Cardputer Zero.

中文定义：

`cardputer-zero-shell` 是面向 Cardputer Zero 的轻量图形 Shell。它在用户登录之后运行，运行在真实登录用户的普通会话中。它不负责启动、登录、PAM、用户创建、权限配置或 OS 定制；它只负责小屏图形桌面、APPLaunch 兼容应用扫描、应用启动和登录后的交互体验。

## Intent

`cardputer-zero-shell` 的意图是把 `M5CardputerZero-Launcher` 中“用户登录后的图形启动器能力”抽出来，形成一个边界更干净、权限更规范、可以独立演进的 Zero GUI Shell。

它不是：

- OS profile
- login manager
- greeter
- system customizer
- app store backend
- root service

它是：

- post-login GUI shell
- small-screen launcher
- APPLaunch-compatible desktop entry scanner
- application runner
- user-session UI surface

## System Boundary

系统边界必须保持清楚：

```text
cardputer-zero-os
  boot splash / GUI greeter / PAM / user session / permissions / zero-helper

cardputer-zero-shell
  post-login GUI launcher / app scan / app launch / small-screen interaction
```

`cardputer-zero-shell` owns:

- ZeroShell home screen
- carousel navigation
- `/usr/share/APPLaunch/applications/*.desktop` scanning
- `Terminal=true` terminal page
- `Terminal=false` blocking app launch
- basic status display
- reload and power menu UI
- restricted helper calls through `zero-helper`

`cardputer-zero-shell` does not own:

- user creation
- password storage
- PAM authentication
- GUI greeter
- LightDM or HDMI login
- systemd login services
- udev rules
- OS package installation
- autologin
- running user desktop as root
- scanning normal Linux desktop apps from `/usr/share/applications`

More detail: [docs/intent.md](docs/intent.md)

## Runtime Contract

`cardputer-zero-os` starts ZeroShell from the authenticated user session:

```text
/usr/local/bin/cardputer-zero-session
  -> /opt/cardputer-zero-shell/bin/zero-shell
```

The process must run as the logged-in user:

```sh
ps -eo user,pid,args | grep zero-shell
```

Expected shape:

```text
pi      1234 /opt/cardputer-zero-shell/bin/zero-shell
```

Not acceptable:

```text
root    1234 /opt/cardputer-zero-shell/bin/zero-shell
```

ZeroShell intentionally refuses to run as `root`.

More detail: [docs/runtime.md](docs/runtime.md)

## Application Contract

ZeroShell keeps the APPLaunch application directory contract:

```text
/usr/share/APPLaunch/applications/*.desktop
```

It intentionally does not scan:

```text
/usr/share/applications
```

Supported desktop entry subset:

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

- `Name`
- `Exec`
- `Icon`
- `Terminal`
- `TryExec`
- `Sysplause`

More detail: [docs/application-contract.md](docs/application-contract.md)

## User Experience

The intended user-facing flow is:

```text
Pi OS boots
  -> cardputer-zero-os shows splash / greeter
  -> user logs in through PAM
  -> cardputer-zero-session starts ZeroShell
  -> ZeroShell shows small-screen launcher
  -> user selects and launches APPLaunch-compatible apps
```

Controls in the MVP:

- `Left` / `Right`: select app
- `Enter`: open selected app
- `R`: reload application directory
- `Esc`: power menu
- `Q`: quit shell, mainly for development and recovery

More detail: [docs/user-guide.md](docs/user-guide.md)

## Build

Preferred:

```sh
cmake -S . -B build
cmake --build build
```

If `cmake` is unavailable, `install.sh` can fall back to direct C++ compilation.

## Install

```sh
sudo ./install.sh
```

This installs:

- `/opt/cardputer-zero-shell/bin/zero-shell`
- `/opt/cardputer-zero-shell/scripts/*`
- `/usr/share/APPLaunch/applications/*.desktop`
- `/usr/share/APPLaunch/share/images/*` when local assets exist

It does not configure:

- systemd
- PAM
- users
- LightDM
- getty
- udev
- autologin

More detail: [docs/install.md](docs/install.md)

## Repository Layout

```text
cardputer-zero-shell/
├─ README.md
├─ CMakeLists.txt
├─ install.sh
├─ uninstall.sh
├─ applications/
├─ scripts/
├─ main/
│  ├─ include/zero_shell/
│  └─ src/
└─ docs/
```

Implementation specification: [docs/spec.md](docs/spec.md)

