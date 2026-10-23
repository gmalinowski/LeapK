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

qt_plugin_dir() {
  local candidate path
  for candidate in qtpaths6 /usr/lib/qt6/bin/qtpaths6 /usr/lib/qt6/bin/qtpaths; do
    if command -v "$candidate" >/dev/null 2>&1; then
      path="$("$candidate" --plugin-dir 2>/dev/null || true)"
      if [[ "$path" == /* ]]; then
        printf '%s\n' "$path"
        return 0
      fi
    fi
  done
  if command -v qmake6 >/dev/null 2>&1; then
    path="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    if [[ "$path" == /* ]]; then
      printf '%s\n' "$path"
      return 0
    fi
  fi
  return 1
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
plugin_dir="$(qt_plugin_dir || true)"
if [[ -z "$plugin_dir" || "$plugin_dir" != /* ]]; then
  echo "Unable to determine the Qt 6 plugin directory (qtpaths6 or qmake6)." >&2
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
for key in UseAccentColor AccentOpacity CustomColor EnsurePerimeterContrast BorderWidth OutlineGap \
           AmbilightEnabled AmbilightMode AmbilightOpacity AmbilightFrequency AmbilightDuration AmbilightSaturationBoost AmbilightSaturationLeveling AmbilightFallbackStrength AmbilightMinBrightness AmbilightMaxBrightness AmbilightEnsureContrast \
           BorderRadius FadeCorners DrawInside SmoothEdges GlowEnabled GlowRadius HideOnFullScreen \
           ColorCycleEnabled ColorList ColorTransitionDuration ColorCycleOrder ColorCycleEasing \
           BreatheEnabled BreatheSpeed BreatheIntensity \
           GlitchEnabled GlitchSpeed GlitchIntensity GlitchAnomalyEnabled GlitchFrequency \
           NeonFlickerEnabled NeonFlickerInterval NeonFlickerDuration NeonFlickerIntensity \
           FlashEnabled FlashInterval FlashDuration \
           FadeDuration MenuHideDelay MenuShowDelay WindowHideDelay WindowShowDelay \
           MarkDefaultColor MarkInactiveOpacity MarkInactiveWidthReduction MarkInactiveDashed MarkInactiveDashSmooth \
           HighlightRecentWindow RecentWindowsCount RecentWindowColorMode RecentWindowCustomColor RecentWindowOpacity RecentWindowHueShift RecentWindowSaturation RecentWindowWidthReduction RecentWindowDashed RecentWindowGapGrowth RecentWindowDashSmooth; do
  kwriteconfig6 --file kwinrc --group "$config_group" --key "$key" --delete ''
done
kwriteconfig6 --file kwinrc --group Plugins \
  --key "${effect_id}Enabled" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "ToggleActiveWindowOutline" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "MarkActiveWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "MarkActiveWindowWithColor" --delete ''

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
