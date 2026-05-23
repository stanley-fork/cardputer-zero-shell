# Wayland Task Model

This document defines the task model used by `cardputer-zero-shell`.

## Stack

```text
internal ST7789 display
  -> DRM/KMS output
  -> labwc / wlroots compositor
  -> Wayland or Xwayland app windows
  -> ZeroShell launcher/task UI client
```

ZeroShell is not the compositor. It is a normal Wayland client that presents a
small-screen launcher and task UI.

## Task Identity

A task is a compositor-managed toplevel/window.

It is not primarily:

- a PID,
- a child process,
- a process group,
- a shell wrapper,
- a desktop-entry file.

The desktop entry launches the app and supplies labels/icons. The task exists
when the app creates a window that labwc can manage.

## App Requirement

Launchable apps must create one of:

- a Wayland window,
- an Xwayland window,
- an SDL/GTK/Qt/etc window that appears to the compositor.

The required desktop-entry declaration is:

```ini
X-Zero-Display=wayland
```

or:

```ini
X-Zero-Display=xwayland
```

## Launch Model

Launching is non-blocking:

```text
ZeroShell runs Exec
  -> app process starts as the current user
  -> app creates a window
  -> labwc exposes toplevel metadata
  -> ZeroShell shows running state
```

ZeroShell may later launch apps through `systemd-run --user --scope --collect`
to improve process cleanup. The scope is cleanup metadata, not task identity.

## Task UI

The running badge and task panel are driven by compositor toplevel state.

Minimum task item data:

- app icon,
- display label,
- app id or class,
- active/focused state,
- minimized/background state when available.

The current implementation uses:

```sh
wlrctl toplevel list
wlrctl toplevel focus ...
```

A future implementation can replace this with a direct foreign-toplevel protocol
client or a small OS-side window-state agent.

## Global Keys

Global `Tab` and `Esc` policy belongs to `cardputer-zero-os`, because ordinary
Wayland clients cannot observe global keys while another app has focus.

```text
Tab
  -> zero-shell-control tasks
  -> ZeroShell toggles task UI

short Esc
  -> zero-shell-control minimize-active
  -> labwc minimizes active window
  -> ZeroShell is focused

long Esc
  -> zero-shell-control close-active
  -> labwc asks active window to close
  -> ZeroShell is focused
```

## Acceptance Criteria

- ZeroShell runs as `zero-shell-wayland`.
- ZeroShell runs as the authenticated user.
- App launch does not block the launcher event loop.
- Running state comes from compositor toplevels.
- `Tab` opens and closes the task list.
- `Enter` on a running app focuses the existing task.
- Short `Esc` minimizes the active app and returns to ZeroShell.
- Long `Esc` requests close on the active app and returns to ZeroShell.
