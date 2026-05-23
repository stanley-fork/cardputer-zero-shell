# Relationship To cardputer-zero-os

`cardputer-zero-os` and `cardputer-zero-shell` are separate layers.

## Ownership

`cardputer-zero-os` owns:

- internal DRM/KMS display setup,
- greetd/PAM/logind login,
- labwc session startup,
- global key policy,
- polkit agent,
- `zero-helper`,
- device permissions,
- HDMI/LightDM recovery policy.

`cardputer-zero-shell` owns:

- APPLaunch scanning,
- launcher UI,
- non-blocking app launch,
- running badge,
- running task panel,
- task activation requests.

## Dependency Direction

```text
cardputer-zero-os
  -> starts /opt/cardputer-zero-shell/bin/zero-shell-wayland

cardputer-zero-shell
  -> may request controlled OS actions through zero-helper/polkit
```

The reverse direction is not allowed. Shell must not configure the OS, own
login, create users, or become a privileged service.

## Interface

OS starts the shell inside a real user Wayland session:

```text
/usr/local/bin/cardputer-zero-session
  -> /usr/local/bin/cardputer-zero-labwc-session
  -> labwc -S /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

Shell expects:

- `WAYLAND_DISPLAY`,
- `XDG_RUNTIME_DIR`,
- `/usr/share/APPLaunch/applications`,
- `/usr/share/APPLaunch/share/images`,
- `wlrctl` for current task control.

Global `Tab` and `Esc` behavior is delivered through:

```text
/usr/local/bin/zero-shell-control
```
