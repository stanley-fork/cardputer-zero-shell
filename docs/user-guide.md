# User Guide

ZeroShell is the launcher shown after login on the Cardputer Zero internal
screen.

## Home

The home screen shows the installed APPLaunch apps in a three-card carousel.

| Key | Behavior |
| --- | --- |
| `Left` / `Right` | Select app. |
| `Enter` | Open selected app, or focus it if already running. |
| `Tab` | Toggle the running task panel. |
| `R` | Reload app entries. |

## Running Tasks

The task panel lists windows currently visible to the compositor.

| Key | Behavior |
| --- | --- |
| `Up` / `Down` | Select task. |
| `Enter` | Focus selected task. |
| `Tab` / `Esc` | Close the task panel. |

## Global App Controls

These controls are provided by `cardputer-zero-os`:

| Key | Behavior |
| --- | --- |
| Short `Esc` | Minimize active app and return to ZeroShell. |
| Long `Esc` | Ask active app to close and return to ZeroShell. |

## Apps

An app appears when its package installs:

```text
/usr/share/APPLaunch/applications/<app>.desktop
```

The app must declare:

```ini
X-Zero-Display=wayland
```

or:

```ini
X-Zero-Display=xwayland
```

ZeroShell does not include fixed apps. Terminal, Settings, Files, App Store,
System Monitor, and HDMI tools are normal app entries when installed.
