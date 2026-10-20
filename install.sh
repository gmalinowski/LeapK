#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

target_ref="${REF:-${BRANCH:-}}"
forward_args=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --main|--beta|--dev)
      target_ref="main"
      shift
      ;;
    --ref|--branch|--tag)
      if [[ $# -lt 2 ]]; then
        echo "Option $1 requires an argument." >&2
        exit 2
      fi
      target_ref="$2"
      shift 2
      ;;
    --ref=*|--branch=*|--tag=*)
      target_ref="${1#*=}"
      shift
      ;;
    *)
      forward_args+=("$1")
      shift
      ;;
  esac
done

# If executed inside a repository checkout without a requested remote ref, run the local installer
script_dir="$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd)" || script_dir=""
if [[ -z "$target_ref" && -n "$script_dir" && -f "$script_dir/install-active-window-outline.sh" && -f "$script_dir/VERSION" ]]; then
  exec bash "$script_dir/install-active-window-outline.sh" "${forward_args[@]}"
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

if [[ "$target_ref" == "main" || "$target_ref" == "master" ]]; then
  echo "Installing Active Window Outline from branch '$target_ref' (latest development)…"
  archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/heads/${target_ref}.tar.gz"
elif [[ -n "$target_ref" ]]; then
  echo "Installing Active Window Outline from reference '$target_ref'…"
  archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/tags/${target_ref}.tar.gz"
else
  echo "Resolving latest release of Active Window Outline…"
  latest_tag="$(get_latest_release_tag || true)"
  if [[ -n "$latest_tag" ]]; then
    echo "Downloading Active Window Outline $latest_tag from GitHub…"
    archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/tags/${latest_tag}.tar.gz"
  else
    echo "Could not resolve latest release; downloading latest main branch from GitHub…"
    archive_url="https://github.com/gmalinowski/kwin-active-window-outline/archive/refs/heads/main.tar.gz"
  fi
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
  bash install-active-window-outline.sh "${forward_args[@]}" </dev/tty
else
  bash install-active-window-outline.sh "${forward_args[@]}"
fi
