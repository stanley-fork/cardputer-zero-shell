# ZeroShell Intent

本文档定义 `cardputer-zero-shell` 的业务意图、非意图和系统边界。

## Core Intent

`cardputer-zero-shell` 的核心意图是：

```text
把 M5CardputerZero-Launcher 中“用户登录后的图形启动器能力”抽出来，
变成一个边界更干净、权限更规范、可独立演进的 Zero GUI Shell。
```

它面向的是登录成功之后的世界：

```text
authenticated Linux user session
  -> cardputer-zero-shell
  -> small-screen GUI launcher
  -> APPLaunch-compatible application launch
```

它不面向：

```text
boot
login
PAM authentication
user creation
OS permission setup
system service orchestration
```

## Why This Exists

旧的 `M5CardputerZero-Launcher` 同时承担了过多历史角色：

- 图形 launcher
- 内置页面集合
- 应用扫描
- 应用启动
- 部分系统控制
- 固定工具入口
- 历史路径和用户假设

这些东西混在一起会导致两个问题：

- 登录后的 GUI launcher 容易被误认为 OS 或登录器。
- 用户桌面容易重新滑向 root service 或固定用户模型。

ZeroShell 要保留的是登录后的 GUI launcher 能力，而不是旧 Launcher 的全部历史职责。

## Positive Definition

`cardputer-zero-shell` is:

- a post-login GUI shell
- a Cardputer Zero small-screen launcher
- an APPLaunch-compatible `.desktop` scanner
- an app runner
- a status and interaction surface
- a normal user-session process

中文：

- 登录后的图形桌面
- Cardputer Zero 小屏启动器
- APPLaunch 兼容应用扫描器
- 应用启动器
- 状态栏和键盘交互界面
- 普通用户会话进程

## Negative Definition

`cardputer-zero-shell` is not:

- an OS profile
- a display manager
- a greeter
- a PAM client
- a user database
- a systemd login service
- a system package manager
- a privilege manager
- a full Linux desktop environment
- an app store backend

中文：

- 不是 OS
- 不是登录器
- 不做 PAM 认证
- 不创建用户
- 不配置 systemd
- 不配置 udev
- 不安装系统包
- 不管理 Linux 权限
- 不替代 Pi OS 的桌面环境
- 不作为 root 拉起用户桌面

## Boundary With cardputer-zero-os

`cardputer-zero-os` 负责：

- userspace startup splash
- Zero GUI greeter
- existing system user selection
- PAM authentication
- session launch
- device permissions
- `zero-helper`
- recovery path

`cardputer-zero-shell` 负责：

- post-login Zero home screen
- APPLaunch-compatible application discovery
- carousel navigation
- terminal and external app launch
- application reload
- status display
- power/display menu UI that calls `zero-helper`

The dependency direction is:

```text
cardputer-zero-os -> starts cardputer-zero-shell
cardputer-zero-shell -> may call zero-helper for restricted root actions
```

The dependency direction must not become:

```text
cardputer-zero-shell -> configures cardputer-zero-os
cardputer-zero-shell -> owns login
cardputer-zero-shell -> owns users
cardputer-zero-shell -> runs as root to manage everything
```

## Boundary With APPLaunch

ZeroShell keeps the existing APPLaunch application directory contract:

```text
/usr/share/APPLaunch/applications
/usr/share/APPLaunch/share/images
/usr/share/APPLaunch/share/font
/usr/share/APPLaunch/share/audio
```

This is a compatibility contract, not a statement that ZeroShell is APPLaunch.

In other words:

```text
APPLaunch directory contract = compatibility surface
ZeroShell = Cardputer Zero post-login GUI shell
```

ZeroShell may eventually use LVGL, framebuffer, SDL for development, or another backend. The application contract should remain stable across those implementation changes.

## Design Consequences

Because ZeroShell is a post-login GUI shell:

- it must run as the authenticated user,
- it must not store passwords,
- it must not create or mutate users,
- it must not disable LightDM or HDMI login,
- it must not scan all desktop applications by default,
- it must not become the place where OS customization logic accumulates.

Because ZeroShell is small-screen first:

- application discovery is intentionally narrow,
- UI density is optimized for 320x170 class displays,
- keyboard navigation matters more than mouse interaction,
- a five-slot carousel is acceptable for MVP,
- full XDG desktop compliance is not required in the first version.

## Non-Goals

MVP non-goals:

- full XDG desktop spec support
- multi-window system
- compositor integration
- app installation marketplace
- graphical file manager
- graphical settings center
- graphical system monitor
- advanced network manager UI
- user account management
- password prompts
- background root daemon

Those can exist as separate applications launched through `.desktop` entries, not as built-in ZeroShell responsibilities.

