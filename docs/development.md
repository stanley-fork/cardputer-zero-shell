# Development

## Local Build

```sh
cmake -S . -B build
cmake --build build
```

## Direct Compiler Path

`install.sh` can compile without CMake when the target has a C++ compiler,
`wayland-scanner`, `xdg-shell.xml`, `libpng`, and `libwayland-client`.

## Run In A Wayland Session

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell-wayland
```

Do not run ZeroShell with `sudo`.

## Useful Overrides

| Variable | Meaning |
| --- | --- |
| `ZEROSHELL_APPLICATIONS_DIR` | App desktop-entry directory. |
| `ZEROSHELL_APPLAUNCH_DIR` | APPLaunch data root. |
| `ZERO_SHELL_WAYLAND_DEBUG` | Extra Wayland key/debug logging. |

## Coding Rules

- Keep app-specific labels and launch behavior in desktop entries.
- Keep OS/session/input/polkit behavior in `cardputer-zero-os`.
- Keep tasks based on compositor windows, not child PIDs.
- Prefer adding small protocol integrations over adding app-name special cases.
