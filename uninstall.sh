#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# If executed inside a repository checkout, run the local uninstaller
script_dir="$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd)" || script_dir=""
if [[ -n "$script_dir" && -f "$script_dir/uninstall-active-window-outline.sh" ]]; then
  exec bash "$script_dir/uninstall-active-window-outline.sh" "$@"
fi

tmp_script="$(mktemp /tmp/uninstall-active-window-outline.XXXXXX.sh)"
cleanup() {
  rm -f "$tmp_script"
}
trap cleanup EXIT

script_url="https://raw.githubusercontent.com/gmalinowski/kwin-active-window-outline/main/uninstall-active-window-outline.sh"

if command -v curl >/dev/null 2>&1; then
  curl -sSL "$script_url" -o "$tmp_script"
elif command -v wget >/dev/null 2>&1; then
  wget -qO "$tmp_script" "$script_url"
else
  echo "Neither curl nor wget was found. Please install curl or wget." >&2
  exit 1
fi

if [[ -r /dev/tty && -w /dev/tty ]]; then
  bash "$tmp_script" "$@" </dev/tty
else
  bash "$tmp_script" "$@"
fi
