# ZeroShell MVP Specification

本文档定义当前 ZeroShell MVP 的实现规格。

## Scope

MVP scope:

- framebuffer GUI launcher
- APPLaunch-compatible desktop scanner
- five-slot carousel
- keyboard navigation
- internal PTY terminal page for `Terminal=true`
- blocking app runner for `Terminal=false`
- status bar
- app reload
- power menu through `zero-helper`
- root refusal

Out of scope:

- login UI
- PAM
- OS setup
- udev setup
- user creation
- package installation
- full XDG desktop spec
- multi-window compositor
- graphical app store
- full terminal emulator
- hard-coded built-in app pages

## Architecture

Current source layout:

```text
main/include/zero_shell/
  app_catalog.hpp
  framebuffer_canvas.hpp
  input_device.hpp
  process_runner.hpp
  pty_terminal.hpp
  shell_ui.hpp
  status.hpp

main/src/
  app_catalog.cpp
  framebuffer_canvas.cpp
  input_device.cpp
  main.cpp
  process_runner.cpp
  pty_terminal.cpp
  shell_ui.cpp
  status.cpp
```

## Modules

### AppCatalog

Responsible for:

- scanning the applications directory,
- parsing `.desktop` files,
- applying `TryExec`,
- de-duplicating by `Exec`,
- returning app entries to UI.

Not responsible for:

- installing apps,
- validating full XDG desktop spec,
- app store metadata,
- privilege checks.

### FramebufferCanvas

Responsible for:

- opening framebuffer device,
- drawing basic rectangles,
- drawing text with built-in bitmap font,
- drawing placeholder icon tiles,
- presenting pixels to framebuffer.

Not responsible for:

- HDMI display,
- compositor integration,
- image decoding in MVP,
- LVGL runtime.

### InputDevice

Responsible for:

- opening keyboard evdev,
- mapping navigation keys,
- mapping basic text input for terminal page.

Not responsible for:

- keyboard layout configuration,
- udev permissions,
- all IME/input methods,
- mouse/touch UI.

### ShellUi

Responsible for:

- main event loop,
- app carousel state,
- status refresh,
- app launching,
- power menu.

Not responsible for:

- authentication,
- user session creation,
- OS recovery.

### PtyTerminal

Responsible for:

- creating a PTY with `forkpty`,
- running terminal commands,
- rendering simple terminal output,
- forwarding basic keyboard input.

Not responsible for:

- complete VT100/xterm emulation,
- high fidelity curses rendering,
- scrollback history,
- terminal multiplexing.

### ProcessRunner

Responsible for:

- running blocking shell commands,
- calling `zero-helper` for allowed privileged actions.

Not responsible for:

- arbitrary sudo,
- package installation,
- systemd control,
- process supervision.

### Status

Responsible for:

- current time,
- basic WiFi signal detection,
- basic battery capacity detection.

Not responsible for:

- complete power management,
- network configuration,
- battery calibration.

## UI Specification

Resolution target:

```text
320x170 class internal display
```

Home layout:

```text
top status bar
five-slot carousel
mode/status hint
bottom control hint
```

Carousel:

```text
left outer / left / center / right / right outer
```

Center item is selected.

## Keyboard Specification

Home:

| Key | Behavior |
| --- | --- |
| `Left` | Previous app |
| `Right` | Next app |
| `Enter` | Launch app |
| `R` | Reload apps |
| `Esc` | Power menu |
| `Q` | Quit shell |

Power menu:

| Key | Behavior |
| --- | --- |
| `Up` | Previous action |
| `Down` / `Left` / `Right` | Next action |
| `Enter` | Execute selected action |
| `Esc` | Close menu |

Terminal page:

| Key | Behavior |
| --- | --- |
| text keys | Forward text to PTY |
| `Enter` | `\r` |
| `Backspace` | DEL |
| arrow keys | ANSI cursor sequences |
| `Esc` | Terminate command and return |

## Security And Privilege Specification

ZeroShell must:

- refuse to run as root,
- launch apps as the current user,
- call restricted `zero-helper` for privileged system actions,
- avoid arbitrary sudo/systemctl/apt wrappers.

ZeroShell must not:

- create users,
- store passwords,
- read password hashes,
- call PAM,
- grant itself device permissions,
- run the user desktop as root.

## Compatibility Specification

Stable compatibility surface:

```text
/usr/share/APPLaunch/applications/*.desktop
```

MVP-compatible fields:

- `Name`
- `Exec`
- `Icon`
- `Terminal`
- `TryExec`
- `Sysplause`

Future UI backends should preserve this application contract.

## Current Implementation Note

The current MVP uses direct framebuffer drawing instead of importing the old APPLaunch LVGL code.

Reason:

旧 APPLaunch 的 LVGL/SConstruct implementation is heavily coupled to SDK build logic, built-in pages, AppStore assumptions, RadioLib, fixed tools and historical paths. Importing it wholesale would reintroduce the boundary drift this repo is meant to remove.

The design keeps the application contract stable so that a future LVGL backend can replace the framebuffer UI without changing `.desktop` semantics.

