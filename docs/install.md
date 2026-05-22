# Install Guide

本文档说明如何构建、安装、卸载和验证 ZeroShell。

## Build With CMake

Preferred build:

```sh
cmake -S . -B build
cmake --build build
```

Output:

```text
build/zero-shell
```

## Install

Install as root:

```sh
sudo ./install.sh
```

Installed files:

```text
/opt/cardputer-zero-shell/bin/zero-shell
/opt/cardputer-zero-shell/scripts/*
/usr/share/APPLaunch/applications/*.desktop
/usr/share/APPLaunch/share/images/*
```

Image assets are installed only when local `main/assets/images` exists.

## CMake Fallback

If `cmake` is unavailable, `install.sh` falls back to direct C++ compilation using:

```text
c++ -std=c++17 ... -lutil
```

This fallback exists because minimal Raspberry Pi OS images may have a C++ compiler but not CMake.

## What install.sh Does Not Do

`install.sh` does not configure:

- systemd
- PAM
- users
- LightDM
- getty
- udev
- autologin
- quiet boot
- splash
- greeter

Those are `cardputer-zero-os` responsibilities.

## Uninstall

```sh
sudo ./uninstall.sh
```

This removes the binary, default scripts and default `.desktop` files installed by this repo.

It does not remove:

- custom user-installed `.desktop` files,
- APPLaunch data directories,
- OS services,
- user accounts.

## Verify Installation

Check binary:

```sh
ls -l /opt/cardputer-zero-shell/bin/zero-shell
file /opt/cardputer-zero-shell/bin/zero-shell
```

Check default apps:

```sh
find /usr/share/APPLaunch/applications -maxdepth 1 -type f -name '*.desktop' -printf '%f\n' | sort
```

Expected default entries:

```text
app-store.desktop
files.desktop
hdmi.desktop
settings.desktop
system-monitor.desktop
terminal.desktop
```

Check that it refuses root:

```sh
sudo /opt/cardputer-zero-shell/bin/zero-shell
```

Expected:

```text
zero-shell: refusing to run as root. Start it from a logged-in user session.
```

Check user-session launch:

```sh
timeout 3s /usr/local/bin/cardputer-zero-session
```

Expected:

- no “shell not found” recovery message,
- no root refusal message,
- timeout exits after shell enters the main loop.

## Remote Deploy Example

Example for deploying to a Raspberry Pi:

```sh
tar --exclude=build -czf /tmp/cardputer-zero-shell.tgz .
scp /tmp/cardputer-zero-shell.tgz pi@192.168.50.35:/tmp/
ssh pi@192.168.50.35 'rm -rf /tmp/cardputer-zero-shell-src && mkdir -p /tmp/cardputer-zero-shell-src'
ssh pi@192.168.50.35 'tar -xzf /tmp/cardputer-zero-shell.tgz -C /tmp/cardputer-zero-shell-src'
ssh pi@192.168.50.35 'cd /tmp/cardputer-zero-shell-src && sudo ./install.sh'
```

Do not copy an x86 build product to the Pi. Build on the target device or cross-compile deliberately.

