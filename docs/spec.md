# ZeroShell Specification

## Scope

ZeroShell is the Cardputer Zero post-login Wayland launcher.

It provides:

- 320x170 launcher UI,
- APPLaunch desktop-entry scan,
- PNG icon loading,
- status display,
- non-blocking app launch,
- running badge,
- running task panel,
- compositor task activation requests.

It does not provide:

- login,
- PAM,
- user management,
- OS configuration,
- udev/group policy,
- polkit policy,
- compositor implementation,
- app-store backend,
- built-in fixed applications.

## Build Target

The repository builds one runtime executable:

```text
zero-shell-wayland
```

Installed path:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
```

## Source Map

| File | Responsibility |
| --- | --- |
| `main/src/zero_shell_wayland.cpp` | Wayland client, UI rendering, key handling while focused, app launch, task polling. |
| `main/include/zero_shell/app_catalog.hpp` | APPLaunch entry data model. |
| `main/src/app_catalog.cpp` | Desktop-entry parsing, `TryExec`, path resolution. |
| `main/include/zero_shell/image.hpp` / `main/src/image.cpp` | PNG icon loading. |
| `main/include/zero_shell/status.hpp` / `main/src/status.cpp` | Time, WiFi, and battery status. |

## UI

The home screen contains:

- 20 px top bar,
- three-card carousel,
- app name/status text,
- 20 px bottom bar with task shortcut and controls.

The task panel shows compositor-visible windows, excluding ZeroShell itself.

## App Launch

ZeroShell launches the selected entry only when:

- `Name` is present,
- `Exec` is present,
- `TryExec` is available when declared,
- `X-Zero-Display` is `wayland` or `xwayland`.

If a matching running task already exists, `Enter` focuses that task instead of
starting another instance.

## App Matching

Running task matching uses:

- `X-Zero-AppId`,
- `StartupWMClass`,
- desktop-entry id,
- display name/title matching when no stronger hint exists.

ZeroShell must not special-case concrete app names.

## Security

ZeroShell must:

- refuse to run as root,
- launch apps as the current user,
- avoid arbitrary privilege wrappers,
- rely on `cardputer-zero-os` for privileged helper policy.

## Environment

Supported development variables:

- `ZEROSHELL_APPLICATIONS_DIR`
- `ZEROSHELL_APPLAUNCH_DIR`
- `ZERO_SHELL_WAYLAND_DEBUG`

Production paths:

```text
/usr/share/APPLaunch/applications
/usr/share/APPLaunch/share/images
```

## Invariants

- The shell is a Wayland client.
- The compositor owns output and window management.
- A task is a compositor toplevel/window.
- Fixed tools are desktop entries, not built-in shell pages.
- Shell failure is handled by the OS/session layer, not by an alternate shell
  implementation.
