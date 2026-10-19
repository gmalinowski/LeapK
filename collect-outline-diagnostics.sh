#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# Read-only: no KWin configuration changes, reloads, or elevated privileges.
qdbus_bin=''
for candidate in qdbus6 /usr/lib/qt6/bin/qdbus /usr/lib64/qt6/bin/qdbus qdbus; do
    if command -v "$candidate" >/dev/null 2>&1; then
        qdbus_bin=$(command -v "$candidate")
        break
    fi
done
if [[ -z "$qdbus_bin" ]]; then
    printf '%s\n' 'Qt 6 qdbus was not found. Install the Qt 6 tools package for your distribution.' >&2
    exit 1
fi

snapshot() {
    "$qdbus_bin" org.kde.KWin /Effects org.kde.kwin.Effects.debug active-window-outline windows
}
if ! initial=$(snapshot); then
    printf '%s\n' 'Cannot read KWin diagnostics. Run this in your Plasma session, without sudo.' >&2
    exit 1
fi
if [[ "$initial" != version=*build=* ]]; then
    printf '%s\n' 'The diagnostic build is not loaded. Enable the effect and log out/in after installing it.' >&2
    printf '%s\n' "$initial" >&2
    exit 1
fi

log_file=$(mktemp "${TMPDIR:-/tmp}/active-window-outline-diagnostics.XXXXXX.log")
printf 'Saving to: %s\n' "$log_file"
printf '%s\n' 'Recording for 20 seconds. Switch to a window, open and close its context menu,' \
    'then move it towards and away from the floating panel.' \
    'Only geometry, window flags and the effect build ID are recorded, not window titles or content.'
printf '%s\n%s\n' 'Active Window Outline diagnostics' "$initial" > "$log_file"
previous=$initial
for ((sample = 1; sample <= 40; sample++)); do
    sleep 0.5
    if ! current=$(snapshot); then
        printf '\nCapture failed at sample %s.\n' "$sample" >> "$log_file"
        printf 'Capture interrupted. Partial log: %s\n' "$log_file" >&2
        exit 1
    fi
    if [[ "$current" != "$previous" ]]; then
        printf '\n--- sample %s ---\n%s\n' "$sample" "$current" >> "$log_file"
        previous=$current
    fi
done
printf 'Done. Attach this file to the bug report: %s\n' "$log_file"
