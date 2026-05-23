# APPLaunch Application Contract

ZeroShell discovers apps through APPLaunch-compatible desktop entries.

## Directory

Production scan directory:

```text
/usr/share/APPLaunch/applications
```

ZeroShell scans:

```text
/usr/share/APPLaunch/applications/*.desktop
```

It does not scan `/usr/share/applications`.

## Data Paths

The APPLaunch data root is:

```text
/usr/share/APPLaunch
```

Common icon path:

```text
/usr/share/APPLaunch/share/images
```

Relative icon paths such as `share/images/app.png` resolve under the data root.

## Desktop Entry Subset

Example:

```ini
[Desktop Entry]
Name=LoFiBox
TryExec=lofibox
Exec=lofibox
Icon=share/images/lofibox.png
X-Zero-Display=xwayland
StartupWMClass=lofibox
```

Supported fields:

| Field | Required | Meaning |
| --- | --- | --- |
| `Name` | yes | Display name. |
| `Exec` | yes | Command to launch. |
| `Icon` | no | APPLaunch-compatible icon path. |
| `TryExec` | no | Hide entry when the command is unavailable. |
| `X-Zero-ShortName` | no | Short label for the 320x170 UI. |
| `StartupWMClass` | no | Xwayland/X11 matching hint. |
| `X-Zero-AppId` | no | Wayland app id matching hint. |
| `X-Zero-Display` | yes | Runtime display contract: `wayland` or `xwayland`. |

Unknown fields are ignored.

## Display Contract

`X-Zero-Display` is required. Valid values:

```ini
X-Zero-Display=wayland
```

```ini
X-Zero-Display=xwayland
```

The app must create a compositor-visible window. That is what allows labwc to
minimize, close, focus, stack, and expose it as a task.

## Name And Short Name

`Name` is the normal label:

```ini
Name=App Store
```

`X-Zero-ShortName` is optional and lets the package choose a compact launcher
label:

```ini
X-Zero-ShortName=STORE
```

ZeroShell must not hard-code app-specific aliases. Packages own their displayed
names through their desktop entries.

## Exec

`Exec` is run through `/bin/sh -lc` as the current authenticated user:

```ini
Exec=lofibox
```

Launch is non-blocking. ZeroShell returns to its event loop while the app creates
its window.

## TryExec

If `TryExec` is present, ZeroShell hides the app when the command is not
available:

```ini
TryExec=lofibox
```

If `TryExec` is missing, ZeroShell checks the first token of `Exec`.

## Window Matching

ZeroShell matches running task state to an app by:

- `X-Zero-AppId`,
- `StartupWMClass`,
- desktop-entry id,
- display name/title matching when needed.

Wayland-native apps should set a stable app id. Xwayland apps should set a
stable `StartupWMClass` when possible.

## Duplicate Handling

If multiple entries have the same `Exec`, the first loaded entry wins. Entries
are loaded in sorted path order.

## Installing An App

Install a desktop entry:

```sh
sudo install -m 0644 my-tool.desktop /usr/share/APPLaunch/applications/my-tool.desktop
```

Install its icon:

```sh
sudo install -m 0644 my-tool.png /usr/share/APPLaunch/share/images/my-tool.png
```

Then press `R` in ZeroShell, or wait for directory metadata polling to reload
the app list.

## Fixed Tools

Settings, Files, Terminal, App Store, System Monitor, HDMI tools, and other
utilities are app packages. They appear through desktop entries. They are not
built into ZeroShell.
