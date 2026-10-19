#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

run_privileged() {
  if command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  elif command -v doas >/dev/null 2>&1; then
    doas "$@"
  else
    echo "Removing system plugin files requires sudo or doas." >&2
    exit 1
  fi
}

effect_id="active-window-outline"
config_group="Effect-active-window-outline"

echo "[1/4] Disabling Active Window Outline…"
kwriteconfig6 --file kwinrc --group Plugins \
  --key "${effect_id}Enabled" false
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    "$effect_id" >/dev/null 2>&1 || true
fi

echo "[2/4] Removing the effect files…"
plugin_dir="$(qtpaths6 --plugin-dir 2>/dev/null || true)"
if [[ -z "$plugin_dir" || "$plugin_dir" != /* ]]; then
  echo "Unable to determine the Qt 6 plugin directory." >&2
  exit 1
fi

plugin_files=(
  "$plugin_dir/kwin/effects/plugins/active-window-outline.so"
  "$plugin_dir/kwin/effects/configs/kwin_active-window-outline_config.so"
  "$plugin_dir/kwin-x11/effects/plugins/active-window-outline.so"
  "$plugin_dir/kwin-x11/effects/configs/kwin_active-window-outline_config.so"
)

installed_files=()
for file in "${plugin_files[@]}"; do
  if [[ -e "$file" ]]; then
    installed_files+=("$file")
  fi
done

if ((${#installed_files[@]})); then
  run_privileged rm -f -- "${installed_files[@]}"
else
  echo "No installed Active Window Outline files were found."
fi

echo "[3/4] Removing the effect settings…"
for key in UseAccentColor AccentOpacity CustomColor BorderWidth OutlineGap \
           BorderRadius DrawInside SmoothEdges GlowEnabled GlowRadius; do
  kwriteconfig6 --file kwinrc --group "$config_group" --key "$key" --delete ''
done
kwriteconfig6 --file kwinrc --group Plugins \
  --key "${effect_id}Enabled" --delete ''

echo "[4/4] Refreshing KDE…"
if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6
fi
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
fi

echo
echo "Done. Active Window Outline has been removed."
echo "Build dependencies and the downloaded source folder were left untouched."
