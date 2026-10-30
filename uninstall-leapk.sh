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

effect_id="leapk"
config_group="Effect-leapk"

echo "[1/4] Disabling LeapK…"
kwriteconfig6 --file kwinrc --group Plugins \
  --key "leapkEnabled" false
kwriteconfig6 --file kwinrc --group Plugins \
  --key "active-window-outlineEnabled" false
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    "leapk" >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    "active-window-outline" >/dev/null 2>&1 || true
fi

# Stop background tray processes and systemd user services
if command -v systemctl >/dev/null 2>&1; then
  systemctl --user stop leapk-tray.service kleap-tray.service active-window-outline-tray.service 2>/dev/null || true
fi
killall leapk-tray kleap-tray active-window-outline-tray 2>/dev/null || true

echo "[2/4] Removing the effect files and desktop launchers…"
plugin_dir="$(qt_plugin_dir || true)"
if [[ -z "$plugin_dir" || "$plugin_dir" != /* ]]; then
  echo "Unable to determine the Qt 6 plugin directory (qtpaths6 or qmake6)." >&2
  exit 1
fi

system_files=(
  "$plugin_dir/kwin/effects/plugins/leapk.so"
  "$plugin_dir/kwin/effects/configs/kwin_leapk_config.so"
  "$plugin_dir/kwin-x11/effects/plugins/leapk.so"
  "$plugin_dir/kwin-x11/effects/configs/kwin_leapk_config.so"
  "$plugin_dir/kwin/effects/plugins/active-window-outline.so"
  "$plugin_dir/kwin/effects/configs/kwin_active-window-outline_config.so"
  "$plugin_dir/kwin-x11/effects/plugins/active-window-outline.so"
  "$plugin_dir/kwin-x11/effects/configs/kwin_active-window-outline_config.so"
  "/usr/libexec/leapk-colorpicker"
  "/usr/lib/x86_64-linux-gnu/libexec/leapk-colorpicker"
  "/usr/lib/libexec/leapk-colorpicker"
  "/usr/local/libexec/leapk-colorpicker"
  "/usr/libexec/kleap-colorpicker"
  "/usr/lib/x86_64-linux-gnu/libexec/kleap-colorpicker"
  "/usr/lib/libexec/kleap-colorpicker"
  "/usr/local/libexec/kleap-colorpicker"
  "/usr/libexec/active-window-outline-colorpicker"
  "/usr/lib/x86_64-linux-gnu/libexec/active-window-outline-colorpicker"
  "/usr/lib/libexec/active-window-outline-colorpicker"
  "/usr/local/libexec/active-window-outline-colorpicker"
  "/usr/local/bin/leapk-colorpicker"
  "/usr/bin/leapk-colorpicker"
  "/usr/local/bin/kleap-colorpicker"
  "/usr/bin/kleap-colorpicker"
  "/usr/local/bin/active-window-outline-colorpicker"
  "/usr/bin/active-window-outline-colorpicker"
  "/usr/local/bin/leapk-tray"
  "/usr/bin/leapk-tray"
  "/usr/local/bin/kleap-tray"
  "/usr/bin/kleap-tray"
  "/usr/local/bin/active-window-outline-tray"
  "/usr/bin/active-window-outline-tray"
  "/usr/local/bin/leapk-settings"
  "/usr/bin/leapk-settings"
  "/usr/local/bin/kleap-settings"
  "/usr/bin/kleap-settings"
  "/usr/local/bin/active-window-outline-settings"
  "/usr/bin/active-window-outline-settings"
  "/usr/local/bin/leapk"
  "/usr/bin/leapk"
  "/usr/local/bin/kleap"
  "/usr/bin/kleap"
  "/usr/share/applications/leapk-tray.desktop"
  "/usr/share/applications/leapk-settings.desktop"
  "/usr/local/share/applications/leapk-tray.desktop"
  "/usr/local/share/applications/leapk-settings.desktop"
  "/usr/share/applications/kleap-tray.desktop"
  "/usr/share/applications/kleap-settings.desktop"
  "/usr/local/share/applications/kleap-tray.desktop"
  "/usr/local/share/applications/kleap-settings.desktop"
  "/usr/share/applications/active-window-outline-tray.desktop"
  "/usr/share/applications/active-window-outline-settings.desktop"
  "/usr/local/share/applications/active-window-outline-tray.desktop"
  "/usr/local/share/applications/active-window-outline-settings.desktop"
  "/usr/share/icons/hicolor/scalable/apps/leapk.svg"
  "/usr/local/share/icons/hicolor/scalable/apps/leapk.svg"
  "/usr/share/icons/hicolor/scalable/apps/kleap.svg"
  "/usr/local/share/icons/hicolor/scalable/apps/kleap.svg"
  "/usr/share/icons/hicolor/scalable/apps/active-window-outline.svg"
  "/usr/local/share/icons/hicolor/scalable/apps/active-window-outline.svg"
)

installed_system_files=()
for file in "${system_files[@]}"; do
  if [[ -e "$file" ]]; then
    installed_system_files+=("$file")
  fi
done

if ((${#installed_system_files[@]})); then
  run_privileged rm -f -- "${installed_system_files[@]}"
fi

# Clean up user-level binaries, desktop files, autostart, and icons
rm -f "$HOME/.local/bin/leapk" \
      "$HOME/.local/bin/leapk-tray" \
      "$HOME/.local/bin/leapk-settings" \
      "$HOME/.local/bin/leapk-colorpicker" \
      "$HOME/.local/bin/kleap" \
      "$HOME/.local/bin/kleap-tray" \
      "$HOME/.local/bin/kleap-settings" \
      "$HOME/.local/bin/kleap-colorpicker" \
      "$HOME/.local/bin/active-window-outline-settings" \
      "$HOME/.local/bin/active-window-outline-tray" \
      "$HOME/.local/bin/active-window-outline-colorpicker" \
      "$HOME/.local/share/applications/leapk-settings.desktop" \
      "$HOME/.local/share/applications/leapk-tray.desktop" \
      "$HOME/.local/share/applications/kleap-settings.desktop" \
      "$HOME/.local/share/applications/kleap-tray.desktop" \
      "$HOME/.local/share/applications/active-window-outline-settings.desktop" \
      "$HOME/.local/share/applications/active-window-outline-tray.desktop" \
      "$HOME/.config/autostart/leapk-tray.desktop" \
      "$HOME/.config/autostart/kleap-tray.desktop" \
      "$HOME/.config/autostart/active-window-outline-tray.desktop" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/leapk.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/kleap.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/active-window-outline.svg"

echo "[3/4] Removing the effect settings…"
for key in UseAccentColor AccentOpacity CustomColor EnsurePerimeterContrast BorderWidth OutlineGap \
           AmbilightEnabled AmbilightMode AmbilightOpacity AmbilightFrequency AmbilightDuration AmbilightSaturationBoost AmbilightSaturationLeveling AmbilightFallbackStrength AmbilightMinBrightness AmbilightMaxBrightness AmbilightEnsureContrast \
           BorderRadius FadeCorners DrawInside SmoothEdges GlowEnabled GlowRadius HideOnFullScreen TrayIconEnabled ActivePreset \
           ColorCycleEnabled ColorList ColorTransitionDuration ColorCycleOrder ColorCycleEasing \
           BreatheEnabled BreatheSpeed BreatheIntensity \
           GlitchEnabled GlitchSpeed GlitchIntensity GlitchAnomalyEnabled GlitchFrequency \
           NeonFlickerEnabled NeonFlickerInterval NeonFlickerDuration NeonFlickerIntensity \
           FlashEnabled FlashInterval FlashDuration \
           CustomShaderEnabled ActiveCustomShaderId ActiveThemeId ActiveThemeParams \
           AiEnabled AiAlwaysSendToAi AiBackend AiLocalModelPath AiApiEndpoint AiApiModel AiApiKey AiApiProviderPreset AiShortcut \
           FadeDuration MenuHideDelay MenuShowDelay WindowHideDelay WindowShowDelay \
           MarkDefaultColor MarkSmartAdaptive MarkRandomColor StylePrecedence MarkInactiveDashGap MarkInactiveWidthReduction MarkInactiveDashed MarkInactiveDashSmooth MarkShowLetterBadges BadgesAlwaysVisible BadgeDisplayMode BadgeWindowBadgesEnabled BadgePosition BadgeHudPosition BadgeSize BadgeMargin BadgeShowIcon BadgeShowThumbnail BadgeWindowShowIcon BadgeWindowShowThumbnail BadgeWindowIconSize BadgeWindowThumbnailWidth BadgeWindowThumbnailHeight BadgeWindowLetterFontSize BadgeWindowLabelFontSize BadgeHudShowIcon BadgeHudShowThumbnail BadgeHudIconSize BadgeHudThumbnailWidth BadgeHudThumbnailHeight BadgeHudLetterFontSize BadgeHudCaptionFontSize BadgeHudLocationFontSize BadgeHudNavFontSize BadgeHudHintsFontSize BadgeHudShowHints BadgeHudIncludeAll BadgeHudGroupWindows BadgeHudGroupBorderWidth BadgeHudGroupFontSize BadgeHudGroupCornerRadius BadgeHudGroupShaderEnabled BadgeLivePreviews BadgeTimeout JumpRecentKey BadgeJumpSize BadgeHudSize BadgeIconSize BadgeThumbnailWidth BadgeThumbnailHeight BadgeWindowThumbnailSize BadgeHudThumbnailSize BadgeCornerRadius BadgeOpacity BadgeHudMarkColorMode BadgeDefaultColor BadgeLetterColorAuto BadgeLetterColor BadgeLabelColorAuto BadgeLabelUnmarkedColor BadgeCaptionColorAuto BadgeCaptionColor BadgeLocationColorAuto BadgeLocationColor BadgeLetterFont BadgeLabelFont BadgeCaptionFont BadgeLocationFont BadgeLetterFontSize BadgeLabelFontSize BadgeCaptionFontSize BadgeLocationFontSize JumpTargets CursorWarpMode SoundFeedbackEnabled SoundFeedbackVolume \
           HighlightRecentWindow RecentWindowsCount RecentWindowColorMode RecentWindowCustomColor RecentWindowOpacity RecentWindowHueShift RecentWindowSaturation RecentWindowWidthReduction RecentWindowDashed RecentWindowGapGrowth RecentWindowDashSmooth; do
  kwriteconfig6 --file kwinrc --group "$config_group" --key "$key" --delete ''
  kwriteconfig6 --file kwinrc --group "Effect-active-window-outline" --key "$key" --delete ''
done
kwriteconfig6 --file kwinrc --group Plugins \
  --key "leapkEnabled" --delete ''
kwriteconfig6 --file kwinrc --group Plugins \
  --key "active-window-outlineEnabled" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "ToggleActiveWindowOutline" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "MarkActiveWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "MarkActiveWindowWithColor" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "OpenWorkspaceManager" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "OpenMarkerManager" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "ToggleHudGrouping" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "TogglePinActiveWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "JumpToWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "JumpToAudioWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "JumpToSearchWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "RecentWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "PrevRecentWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "NextRecentWindow" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "ToggleTilingPresets" --delete ''
kwriteconfig6 --file kglobalshortcutsrc --group kwin \
  --key "ToggleAiPrompt" --delete ''

echo "[4/4] Refreshing KDE…"
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
  if [[ -d /usr/share/applications ]]; then
    run_privileged update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
  fi
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
fi
if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
fi
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
fi

echo
echo "Done. LeapK has been completely uninstalled and removed."
echo "Build dependencies and any local git repository checkouts were left untouched."
