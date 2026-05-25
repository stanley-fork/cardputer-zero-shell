# ZeroShell Specification

## Scope

ZeroShell is the Cardputer Zero post-login Wayland launcher.

It provides:

- 320x170 launcher UI,
- APPLaunch desktop-entry scan,
- launcher category filtering from desktop-entry `Categories=`,
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
| `main/src/zero_shell_wayland.cpp` | Wayland client, UI rendering, key handling while focused, app launch, and `zero-window-agent` task backend client. |
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

The category drawer is a right-side launcher filter panel. It is visually
similar to the task panel, but it is not backed by compositor state. Its source
of truth is APPLaunch desktop-entry metadata:

- `Categories=` is the only category source.
- `All` is always present and means no filter.
- Apps with no usable category appear under `Other`.
- Moving the category selection updates the visible launcher apps immediately.
- `Enter` accepts the selected category and closes the drawer.
- `Esc` or the category shortcut closes the drawer without changing task state.

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

Task facts must come from `zero-window-agent`. Matching desktop metadata to a
reported task is allowed; creating a task from desktop metadata alone is not.

Category facts must come from desktop-entry `Categories=` values. Category
filtering must not use process state, task state, app names, icon names, or
running windows as classification facts.

## Security

ZeroShell must:

- refuse to run as root,
- launch apps as the current user,
- avoid arbitrary privilege wrappers,
- rely on `cardputer-zero-os` for privileged helper policy.
- keep launcher category filtering inside launcher state, not task state.

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
- A launcher category is a filter over desktop-entry `Categories=`.
- `zero-window-agent` is the only production task-state and task-action
  backend.
- ZeroShell must not invoke or parse `wlrctl`.
- ZeroShell must not scan `/proc` to infer running tasks.
- ZeroShell must not treat forked children or process groups as tasks.
- Missing task backend is an explicit offline state, not a fallback trigger.
- Fixed tools are desktop entries, not built-in shell pages.
- Category filtering must not create, infer, activate, minimize, close, or
  otherwise describe a running task.
- Shell failure is handled by the OS/session layer, not by an alternate shell
  implementation.
