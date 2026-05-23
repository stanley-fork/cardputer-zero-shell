# cardputer-zero-shell

`cardputer-zero-shell` is the post-login GUI shell for Cardputer Zero.

It runs after `cardputer-zero-os` has authenticated a real Linux user and
started the internal-screen Wayland session. It is not an OS profile, not a
greeter, not a display manager, and not a privilege boundary. Its job is to show
the Zero launcher, scan APPLaunch desktop entries, launch windowed apps, and
present compositor task state on the 320x170 internal screen.

## Current UI

![Cardputer Zero Shell home](docs/assets/zero-shell-current-home-320x170.png)

The screenshot is from the device runtime. ZeroShell does not ship fake menu
items. An app appears only when its package installs a desktop entry under the
APPLaunch directory.

## Architecture

The internal screen is managed through the standard Linux graphics stack:

```text
authenticated Linux user
  -> cardputer-zero-session
  -> labwc on the Cardputer Zero internal DRM output
  -> /opt/cardputer-zero-shell/bin/zero-shell-wayland
  -> Wayland or Xwayland app windows
```

There is one supported runtime binary:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
```

ZeroShell is a Wayland client. It draws through Wayland shared-memory buffers,
uses `wlrctl` to inspect compositor toplevels, and asks labwc to focus existing
tasks. Display ownership, focus, minimize, close, stacking, and app windows
belong to the compositor layer.

## Boundary

`cardputer-zero-os` owns:

- internal DRM/KMS display setup,
- greetd/PAM/logind login,
- labwc session startup,
- global `Tab` and `Esc` input policy,
- polkit agent and privileged helper policy,
- udev and group permissions,
- HDMI/LightDM recovery policy.

`cardputer-zero-shell` owns:

- launcher UI,
- APPLaunch desktop-entry scanning,
- non-blocking app launch,
- running badges,
- task list display,
- task activation requests.

Applications own their own windows and domain UI. A launchable app must create a
Wayland or Xwayland window so labwc can manage it as a task.

## Runtime Timing

```text
greetd/PAM authenticates an existing Linux user
  -> cardputer-zero-session
  -> cardputer-zero-labwc-session
  -> labwc starts on /dev/dri/cardputer-zero-internal
  -> labwc starts zero-shell-wayland
  -> ZeroShell scans /usr/share/APPLaunch/applications/*.desktop
  -> user selects an app and presses Enter
  -> ZeroShell launches Exec without blocking the launcher
  -> the app creates a Wayland/Xwayland toplevel
  -> labwc manages the window
  -> ZeroShell shows running state from compositor toplevels
```

Global task controls are mediated by `cardputer-zero-os`:

```text
Tab
  -> labwc keybind calls zero-shell-control tasks
  -> ZeroShell toggles RUNNING TASKS

short Esc
  -> zero-key-policy calls zero-shell-control minimize-active
  -> labwc minimizes the active toplevel
  -> ZeroShell is focused

long Esc
  -> zero-key-policy calls zero-shell-control close-active
  -> labwc asks the active toplevel to close
  -> ZeroShell is focused
```

This is a Wayland rule, not an implementation preference: when another app has
keyboard focus, ZeroShell cannot reliably receive global key events by itself.

## Application Contract

ZeroShell scans exactly:

```text
/usr/share/APPLaunch/applications/*.desktop
```

It intentionally does not scan `/usr/share/applications`, because normal Linux
desktop entries are not necessarily usable on the Cardputer Zero internal
screen.

Minimal entry:

```ini
[Desktop Entry]
Name=LoFiBox
Exec=lofibox
TryExec=lofibox
Icon=share/images/lofibox.png
X-Zero-Display=xwayland
StartupWMClass=lofibox
```

Supported fields:

| Field | Meaning |
| --- | --- |
| `Name` | Display name. |
| `Exec` | Command launched by ZeroShell. |
| `Icon` | Icon path. Relative `share/...` paths resolve under `/usr/share/APPLaunch`. |
| `TryExec` | Hide the entry when the command is unavailable. |
| `X-Zero-ShortName` | Optional short launcher label. |
| `StartupWMClass` | Xwayland/X11 matching hint. |
| `X-Zero-AppId` | Wayland app id matching hint. |
| `X-Zero-Display` | Required runtime display contract: `wayland` or `xwayland`. |

`X-Zero-Display` must be either:

```ini
X-Zero-Display=wayland
```

or:

```ini
X-Zero-Display=xwayland
```

Entries without one of those values are not launched by this shell.

## Task Model

In this shell, a task is a compositor-managed toplevel/window.

A task is not primarily:

- a PID,
- a child process,
- a process group,
- a shell wrapper,
- a desktop-entry file.

Those can help with launch and matching, but the task exists when labwc can see
a window. ZeroShell currently reads task state with:

```sh
wlrctl toplevel list
```

and activates tasks with:

```sh
wlrctl toplevel focus app_id:<id>
wlrctl toplevel focus title:<title>
```

Future work may replace command-output parsing with a toplevel protocol client
or a small OS-side window-state agent. The architectural rule stays the same:
task state comes from compositor windows.

## User Controls

Home:

| Key | Behavior |
| --- | --- |
| `Left` / `Right` | Select app. |
| `Enter` | Launch selected app, or focus it if already running. |
| `Tab` | Toggle running task panel. |
| `R` | Reload APPLaunch entries. |

Running task panel:

| Key | Behavior |
| --- | --- |
| `Up` / `Down` | Select task. |
| `Enter` | Focus selected task through labwc. |
| `Tab` / `Esc` | Hide task panel. |

Global app policy:

| Key | Behavior |
| --- | --- |
| Short `Esc` | Minimize active app window and return to ZeroShell. |
| Long `Esc` | Request close on active app window and return to ZeroShell. |

## File Map

| File | Role |
| --- | --- |
| `CMakeLists.txt` | Builds `zero-shell-wayland` and generates xdg-shell protocol code. |
| `install.sh` | Builds and installs `zero-shell-wayland`; creates APPLaunch data directories. |
| `uninstall.sh` | Removes the installed shell binary. |
| `main/include/zero_shell/app_catalog.hpp` | Defines APPLaunch entry metadata used by the Wayland shell. |
| `main/src/app_catalog.cpp` | Parses APPLaunch `.desktop` files, applies `TryExec`, and resolves metadata. |
| `main/include/zero_shell/image.hpp` / `main/src/image.cpp` | PNG loading for launcher icons. |
| `main/include/zero_shell/status.hpp` / `main/src/status.cpp` | Reads time, WiFi, and battery status. |
| `main/src/zero_shell_wayland.cpp` | Wayland UI, non-blocking launch, running badge, task list, and command-file handling. |

## Build

Required packages on the target:

```sh
sudo apt-get install build-essential cmake pkg-config libpng-dev \
  libwayland-dev wayland-protocols
```

Build:

```sh
cmake -S . -B build
cmake --build build
```

Output:

```text
build/zero-shell-wayland
```

`install.sh` can compile directly when CMake is unavailable, but the Wayland
build still needs `wayland-scanner`, `xdg-shell.xml`, `libpng`, and
`libwayland-client`.

## Install

```sh
sudo ./install.sh
```

Installs:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
/usr/share/APPLaunch/applications
/usr/share/APPLaunch/share/images
```

It does not configure systemd, greetd, PAM, LightDM, udev, users, autologin, or
app entries. Those belong to `cardputer-zero-os` or to app packages.

## Verify

Check process identity:

```sh
ps -eo user,pid,args | grep zero-shell-wayland
```

Expected shape:

```text
pi  /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

Check compositor-visible tasks:

```sh
XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 wlrctl toplevel list
```

Example:

```text
cardputer-zero-shell: Cardputer Zero Shell
lofibox: LoFiBox Zero
```

## Documentation Index

- [docs/intent.md](docs/intent.md)
- [docs/relationship-to-os.md](docs/relationship-to-os.md)
- [docs/application-contract.md](docs/application-contract.md)
- [docs/runtime.md](docs/runtime.md)
- [docs/spec.md](docs/spec.md)
- [docs/user-guide.md](docs/user-guide.md)
- [docs/wayland-task-model.md](docs/wayland-task-model.md)
- [docs/install.md](docs/install.md)
- [docs/development.md](docs/development.md)
