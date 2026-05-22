# Development Notes

本文档给后续开发者一个快速入口。

## Local Build

On Linux or WSL:

```sh
cmake -S . -B build
cmake --build build
```

The program is Linux-specific because it uses:

- Linux framebuffer APIs,
- Linux evdev,
- `forkpty`,
- POSIX process APIs.

Windows native build is not a supported runtime target.

## Run In Development

Use a normal user:

```sh
ZEROSHELL_APPLICATIONS_DIR=./applications ./build/zero-shell
```

Useful overrides:

```sh
ZEROSHELL_FBDEV=/dev/fb0
ZEROSHELL_KEYBOARD_DEVICE=/dev/input/event3
ZEROSHELL_APPLAUNCH_DIR=/usr/share/APPLaunch
```

Do not use `sudo` to run ZeroShell. Root refusal is intentional.

## Testing On Pi

Build/install:

```sh
sudo ./install.sh
```

Check binary:

```sh
file /opt/cardputer-zero-shell/bin/zero-shell
```

Expected target:

```text
ARM aarch64
```

Short-run session test:

```sh
timeout 3s /usr/local/bin/cardputer-zero-session
```

This should enter the shell loop and then be killed by `timeout`.

## Adding A Default Tool

Do not add fixed tools directly to C++ carousel setup.

Instead:

1. Add a `.desktop` file under `applications/`.
2. Add a fallback script under `scripts/` if needed.
3. Ensure `install.sh` installs it.
4. Document it if it changes user expectations.

## Adding A System Action

Do not call arbitrary privileged commands from ZeroShell.

Preferred path:

1. Add a restricted action to `zero-helper` in `cardputer-zero-os`.
2. Allow it through the OS sudoers policy.
3. Call only that allowed helper action from ZeroShell.

Example:

```sh
sudo /usr/local/sbin/zero-helper display mirror
```

Avoid:

```sh
sudo systemctl ...
sudo apt ...
sudo sh -c ...
sudo raspi-config
```

## Adding A UI Backend

The app contract should not depend on the rendering backend.

If replacing framebuffer UI with LVGL:

- keep `AppCatalog`,
- keep APPLaunch `.desktop` semantics,
- keep root refusal,
- keep user-session process model,
- keep `zero-helper` privilege boundary,
- avoid importing old built-in pages wholesale.

## Code Style

- Keep modules small and boundary-oriented.
- Prefer explicit runtime contracts over implicit historical assumptions.
- Avoid hard-coded user paths such as `/home/pi` unless they are development examples.
- Treat `/usr/share/APPLaunch` as a compatibility contract, not as proof that all old APPLaunch behavior belongs here.

