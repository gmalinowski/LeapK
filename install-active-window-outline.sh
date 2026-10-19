#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

project_dir="$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$(mktemp -d /tmp/active-window-outline-build.XXXXXX)"
cleanup() {
  if command -v cmake >/dev/null 2>&1; then
    cmake -E remove_directory "$build_dir"
  fi
}
trap cleanup EXIT
version_file="$project_dir/VERSION"
if [[ ! -r "$version_file" ]]; then
  echo "Unable to read the project version from $version_file." >&2
  exit 1
fi
version="$(<"$version_file")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid project version: $version" >&2
  exit 1
fi
plugin_dir="$(qtpaths6 --plugin-dir 2>/dev/null || true)"
installed_plugin="$plugin_dir/kwin/effects/plugins/active-window-outline.so"
updating_existing=false
if [[ -n "$plugin_dir" && -e "$installed_plugin" ]]; then
  updating_existing=true
fi

if [[ ! -r /etc/os-release ]]; then
  echo "Unable to detect the Linux distribution: /etc/os-release is missing." >&2
  exit 1
fi

echo "Active Window Outline v$version"
echo

# shellcheck disable=SC1091
source /etc/os-release
case " ${ID:-} ${ID_LIKE:-} " in
  *" debian "*|*" ubuntu "*) ;;
  *)
    echo "This installer supports TUXEDO OS, Ubuntu, and Debian." >&2
    exit 1
    ;;
esac

echo "[1/5] Installing build dependencies…"
sudo apt install \
  build-essential cmake extra-cmake-modules pkg-config \
  qt6-base-dev qt6-declarative-dev \
  libkf6coreaddons-dev libkf6config-dev libkf6configwidgets-dev \
  libkf6kcmutils-dev libvulkan-dev libdrm-dev \
  kwin-wayland kwin-dev

echo "[2/5] Building Active Window Outline…"
cmake -S "$project_dir" -B "$build_dir" \
  -DKWIN_BACKEND=WAYLAND \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel "$(nproc)"

effect_binary="$build_dir/lib/kwin/effects/plugins/active-window-outline.so"
plugin_info="$(qtplugininfo6 "$effect_binary" 2>&1 || true)"
if [[ "$plugin_info" != *'org.kde.kwin.EffectPluginFactory'* \
   || "$plugin_info" != *'"Id": "active-window-outline"'* \
   || "$plugin_info" != *'"Version": "'"$version"'"'* ]]; then
  echo "The compiled library does not contain valid KWin effect metadata." >&2
  echo "$plugin_info" >&2
  exit 1
fi

echo "[3/5] Unloading the previous version…"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
  active-window-outline >/dev/null 2>&1 || true

echo "[4/5] Installing the KWin effect…"
sudo cmake --install "$build_dir"
kbuildsycoca6

echo "[5/5] Enabling the effect…"
kwriteconfig6 --file kwinrc --group Plugins \
  --key active-window-outlineEnabled true

set_default() {
  local key="$1"
  local value="$2"
  local current
  current="$(kreadconfig6 --file kwinrc --group Effect-active-window-outline \
    --key "$key" --default '')"
  if [[ -z "$current" ]]; then
    kwriteconfig6 --file kwinrc --group Effect-active-window-outline \
      --key "$key" "$value"
  fi
}

set_default UseAccentColor true
set_default BorderWidth 4
set_default OutlineGap 2
set_default BorderRadius 10
set_default DrawInside true

qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
  active-window-outline >/dev/null 2>&1 || true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
  active-window-outline >/dev/null 2>&1 || true

echo
echo "Done. Active Window Outline v$version has been installed."
echo "Configure it in System Settings → Desktop Effects → Active Window Outline."
if [[ "$updating_existing" == true ]]; then
  echo
  echo "IMPORTANT: This was an update of a native KWin plugin."
  echo "Log out and back in once so KWin loads the new library from disk."
else
  echo "If the effect does not appear immediately, log out and back in."
fi
