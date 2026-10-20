#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# If executed inside a repository checkout, run the local installer
script_dir="$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd)" || script_dir=""
if [[ -n "$script_dir" && -f "$script_dir/install-active-window-outline.sh" && -f "$script_dir/VERSION" ]]; then
  exec bash "$script_dir/install-active-window-outline.sh" "$@"
fi

tmp_dir="$(mktemp -d /tmp/active-window-outline-install.XXXXXX)"
cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

get_latest_release_tag() {
  local location=""
  if command -v curl >/dev/null 2>&1; then
    location="$(curl -sI "https://github.com/gmalinowski/kwin-active-window-outline/releases/latest" 2>/dev/null | tr -d '\r' | awk '/^[Ll]ocation:/ {print $2}')"
  elif command -v wget >/dev/null 2>&1; then
    location="$(wget --spider -S "https://github.com/gmalinowski/kwin-active-window-outline/releases/latest" 2>&1 | tr -d '\r' | awk '/^[[:space:]]*[Ll]ocation:/ {print $2}' | tail -n 1)"
  fi
  if [[ "$location" =~ /releases/tag/([^/]+)$ ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
    return 0
  fi
  return 1
}

echo "Resolving latest release of Active Window Outline…"
latest_tag="$(get_latest_release_tag || true)"

if [[ -n "$latest_tag" ]]; then
  echo "Downloading Active Window Outline $latest_tag from GitHub…"
  archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/tags/${latest_tag}.tar.gz"
else
  echo "Could not resolve latest release; downloading latest main branch from GitHub…"
  archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/heads/main.tar.gz"
fi

if command -v curl >/dev/null 2>&1; then
  curl -sSL "$archive_url" | tar -xz -C "$tmp_dir" --strip-components=1
elif command -v wget >/dev/null 2>&1; then
  wget -qO- "$archive_url" | tar -xz -C "$tmp_dir" --strip-components=1
else
  echo "Neither curl nor wget was found. Please install curl or wget." >&2
  exit 1
fi

cd "$tmp_dir"
if [[ -r /dev/tty && -w /dev/tty ]]; then
  bash install-active-window-outline.sh "$@" </dev/tty
else
  bash install-active-window-outline.sh "$@"
fi
