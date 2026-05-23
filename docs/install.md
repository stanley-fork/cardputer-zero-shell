# Install

## Dependencies

```sh
sudo apt-get install build-essential cmake pkg-config libpng-dev \
  libwayland-dev wayland-protocols
```

## Build

```sh
cmake -S . -B build
cmake --build build
```

Output:

```text
build/zero-shell-wayland
```

## Install

```sh
sudo ./install.sh
```

Installed files:

```text
/opt/cardputer-zero-shell/bin/zero-shell-wayland
/usr/share/APPLaunch/applications
/usr/share/APPLaunch/share/images
```

The installer does not install fake app entries and does not configure login,
PAM, labwc, LightDM, udev, or polkit. That is `cardputer-zero-os` responsibility.

## Verify

```sh
ls -l /opt/cardputer-zero-shell/bin/zero-shell-wayland
ps -eo user,pid,args | grep zero-shell-wayland
```

Expected runtime identity:

```text
pi  /opt/cardputer-zero-shell/bin/zero-shell-wayland
```

## Deploy To The Test Device

```sh
tar --exclude=build -czf /tmp/cardputer-zero-shell.tgz .
scp /tmp/cardputer-zero-shell.tgz pi@192.168.50.35:/tmp/
ssh pi@192.168.50.35 'rm -rf /tmp/cardputer-zero-shell-src && mkdir -p /tmp/cardputer-zero-shell-src'
ssh pi@192.168.50.35 'tar -xzf /tmp/cardputer-zero-shell.tgz -C /tmp/cardputer-zero-shell-src'
ssh pi@192.168.50.35 'cd /tmp/cardputer-zero-shell-src && sudo ./install.sh'
```
