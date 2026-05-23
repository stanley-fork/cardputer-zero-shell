# ZeroShell Intent

`cardputer-zero-shell` is the authenticated-user graphical shell for the
Cardputer Zero internal screen.

Its intent is to extract the post-login launcher capability from
`M5CardputerZero-Launcher` and keep it inside a clean Linux desktop boundary:

```text
cardputer-zero-os
  -> authenticates user and starts the internal Wayland session

cardputer-zero-shell
  -> displays launcher and task UI inside that session

applications
  -> provide their own Wayland or Xwayland windows
```

## Positive Definition

ZeroShell is:

- a post-login launcher,
- a Wayland client,
- an APPLaunch-compatible desktop-entry scanner,
- a non-blocking app launcher,
- a small-screen task UI frontend,
- a normal user-session process.

## Negative Definition

ZeroShell is not:

- an OS profile,
- a greeter,
- a display manager,
- a PAM client,
- a user database,
- a systemd login service,
- a package manager backend,
- a privilege manager,
- a compositor,
- a full desktop environment,
- a root-owned user desktop.

## Design Consequences

- ZeroShell must run as the authenticated user.
- ZeroShell must not store passwords or call PAM.
- ZeroShell must not create or mutate users.
- ZeroShell must not configure systemd, udev, LightDM, greetd, or DRM devices.
- ZeroShell must not scan all system desktop entries by default.
- ZeroShell must not hard-code concrete apps such as Settings, Files, Terminal,
  App Store, System Monitor, or LoFiBox.
- Those tools appear only when app packages install APPLaunch desktop entries.

## Stable Compatibility Surface

ZeroShell keeps the APPLaunch directory contract:

```text
/usr/share/APPLaunch/applications/*.desktop
/usr/share/APPLaunch/share/images
```

This is a compatibility surface, not ownership of the whole APPLaunch project.

## Graphics Boundary

ZeroShell does not own the internal display. The internal screen belongs to the
Linux compositor session started by `cardputer-zero-os`:

```text
internal DRM output
  -> labwc
  -> zero-shell-wayland
  -> Wayland/Xwayland app windows
```

The shell is therefore a launcher and task UI, not a display arbitrator.
