#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <build-log> [build-log ...]" >&2
    exit 2
fi

raw="$(mktemp)"
unexpected="$(mktemp)"
trap 'rm -f "$raw" "$unexpected"' EXIT

for log in "$@"; do
    [[ -f "$log" ]] || {
        echo "error: missing warning-audit log: $log" >&2
        exit 2
    }
    grep -Ei 'warning:|CMake Warning' "$log" >> "$raw" || true
done

# Known external packaging diagnostics only:
# - linuxdeploy Debian copyright lookup noise
# - absent Qt translation directory
# - host SpeexDSP debug-info CRC mismatch
# - intentional NO_STRIP notice from linuxdeploy
grep -Ev \
  'Could not find copyright files for file .* using dpkg-query|Translation directory does not exist, skipping deployment|objdump: Warning: Separate debug info file .*libspeexdsp.*CRC does not match - ignoring|\$NO_STRIP environment variable detected, not stripping binaries' \
  "$raw" > "$unexpected" || true

if [[ -s "$unexpected" ]]; then
    echo "UNEXPECTED WARNINGS:"
    sort -u "$unexpected"
    exit 1
fi

echo "Warning audit: PASS (no unexpected warnings)"
