#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${ZERO_SHELL_BUILD_IMAGE:-cardputer-zero-shell-build:bookworm-arm64}
out_dir="$repo_dir/.docker-out"
container=zero-shell-build-export-$$
tmp_dir=${TMPDIR:-/tmp}/zero-shell-build-export-$$

docker build --platform linux/arm64 -f "$repo_dir/docker/Dockerfile.arm64" -t "$image" "$repo_dir"
mkdir -p "$out_dir"
if [ ! -w "$out_dir" ]; then
  echo "Output directory is not writable: $out_dir" >&2
  echo "Remove and recreate it as the current user, then rerun this script." >&2
  exit 1
fi
mkdir -p "$tmp_dir"
docker rm -f "$container" >/dev/null 2>&1 || true
docker create --platform linux/arm64 --name "$container" "$image" >/dev/null
trap 'docker rm -f "$container" >/dev/null 2>&1 || true; rm -rf "$tmp_dir"' EXIT
docker cp "$container:/out/zero-shell-wayland" "$tmp_dir/zero-shell-wayland"
rm -f "$out_dir/zero-shell-wayland"
cp "$tmp_dir/zero-shell-wayland" "$out_dir/zero-shell-wayland"
chmod 0755 "$out_dir/zero-shell-wayland"

echo "$out_dir/zero-shell-wayland"
