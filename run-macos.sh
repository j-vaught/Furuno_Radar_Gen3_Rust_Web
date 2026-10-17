#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")" && pwd)"
storage_volume="${FURUNO_STORAGE_VOLUME:-/Volumes/MacApplications}"
interface="${FURUNO_INTERFACE:-en5}"
radar_address="${FURUNO_RADAR_IP:-172.31.3.226}"
host_address="${FURUNO_HOST_IP:-172.31.1.100}"
binary="${FURUNO_BINARY:-$repo_root/runtime/macos/furuno-server}"

if [[ "$(uname -s)" != Darwin ]]; then
    printf '%s\n' 'This launcher requires macOS.' >&2
    exit 1
fi
if ! /sbin/mount | /usr/bin/grep -Fq " on $storage_volume ("; then
    printf 'Recording volume is not mounted. %s\n' "$storage_volume" >&2
    exit 1
fi
if [[ ! -x "$binary" ]]; then
    printf 'macOS release binary is missing. %s\n' "$binary" >&2
    exit 1
fi

recording_root="${FURUNO_RECORDING_ROOT:-$storage_volume/Radar_papers/data/live/macos-recordings}"
case "$recording_root/" in
    "$storage_volume/"*) ;;
    *) printf '%s\n' 'Recording root must be on the configured recording volume.' >&2; exit 1 ;;
esac
mkdir -p "$recording_root"
export FURUNO_RECORDING_ROOT="$recording_root"

printf 'Interface %s. Host %s. Radar %s.\n' "$interface" "$host_address" "$radar_address"
printf 'Recording root %s.\n' "$recording_root"
exec "$binary" --interface "$interface" --radar-ip "$radar_address" \
    --host-ip "$host_address" web 8080 "$repo_root/web"
