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

qt_plugin_dirs() {
  local candidate path
  local dirs=()
  for candidate in qtpaths6 /usr/lib/qt6/bin/qtpaths6 /usr/lib/qt6/bin/qtpaths; do
    if command -v "$candidate" >/dev/null 2>&1; then
      path="$("$candidate" --plugin-dir 2>/dev/null || true)"
      if [[ "$path" == /* ]]; then
        dirs+=("$path")
        break
      fi
    fi
  done
  if command -v qmake6 >/dev/null 2>&1; then
    path="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    if [[ "$path" == /* ]]; then
      dirs+=("$path")
    fi
  fi
  # Fallback standard system paths
  dirs+=(
    "/usr/lib/qt6/plugins"
    "/usr/lib/x86_64-linux-gnu/qt6/plugins"
    "/usr/local/lib/qt6/plugins"
    "/usr/lib64/qt6/plugins"
  )
  printf '%s\n' "${dirs[@]}" | sort -u
}

effect_id="leapk"
config_group="Effect-leapk"

echo "[1/5] Disabling LeapK and unloading effects…"
kwriteconfig6 --file kwinrc --group Plugins \
  --key "leapkEnabled" false 2>/dev/null || true
kwriteconfig6 --file kwinrc --group Plugins \
  --key "active-window-outlineEnabled" false 2>/dev/null || true

if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "leapk" >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "active-window-outline" >/dev/null 2>&1 || true
fi
if command -v dbus-send >/dev/null 2>&1; then
  dbus-send --session --dest=org.kde.KWin --type=method_call /Effects org.kde.kwin.Effects.unloadEffect string:"leapk" >/dev/null 2>&1 || true
  dbus-send --session --dest=org.kde.KWin --type=method_call /Effects org.kde.kwin.Effects.unloadEffect string:"active-window-outline" >/dev/null 2>&1 || true
fi

# Stop background tray processes and systemd user services
if command -v systemctl >/dev/null 2>&1; then
  systemctl --user stop leapk-tray.service kleap-tray.service active-window-outline-tray.service 2>/dev/null || true
fi
killall leapk-tray kleap-tray active-window-outline-tray leapk-marker-manager leapk-workspace-manager 2>/dev/null || true

echo "[2/5] Removing system-level binaries, plugins, and desktop launchers…"
system_files=()

while IFS= read -r pdir; do
  [[ -z "$pdir" ]] && continue
  system_files+=(
    "$pdir/kwin/effects/plugins/leapk.so"
    "$pdir/kwin/effects/configs/kwin_leapk_config.so"
    "$pdir/kwin-x11/effects/plugins/leapk.so"
    "$pdir/kwin-x11/effects/configs/kwin_leapk_config.so"
    "$pdir/kwin/effects/plugins/active-window-outline.so"
    "$pdir/kwin/effects/configs/kwin_active-window-outline_config.so"
    "$pdir/kwin-x11/effects/plugins/active-window-outline.so"
    "$pdir/kwin-x11/effects/configs/kwin_active-window-outline_config.so"
  )
done < <(qt_plugin_dirs)

# System libexec and bin candidates
for prefix in /usr /usr/local; do
  for libexec_dir in "$prefix/libexec" "$prefix/lib/libexec" "$prefix/lib/x86_64-linux-gnu/libexec"; do
    for bin_name in leapk-custom-marker leapk-workspace-manager leapk-marker-manager leapk-colorpicker \
                    kleap-custom-marker kleap-workspace-manager kleap-marker-manager kleap-colorpicker \
                    active-window-outline-colorpicker active-window-outline-workspace-manager active-window-outline-marker-manager; do
      system_files+=("$libexec_dir/$bin_name")
    done
  done
  for bin_name in leapk leapk-tray leapk-settings leapk-marker-manager leapk-workspace-manager leapk-custom-marker leapk-colorpicker \
                  kleap kleap-tray kleap-settings kleap-marker-manager kleap-workspace-manager kleap-custom-marker kleap-colorpicker \
                  active-window-outline-settings active-window-outline-tray active-window-outline-marker-manager active-window-outline-workspace-manager active-window-outline-colorpicker; do
    system_files+=("$prefix/bin/$bin_name")
  done
  for desktop_name in leapk-settings.desktop leapk-tray.desktop leapk-marker-manager.desktop \
                      kleap-settings.desktop kleap-tray.desktop kleap-marker-manager.desktop \
                      active-window-outline-settings.desktop active-window-outline-tray.desktop active-window-outline-marker-manager.desktop; do
    system_files+=("$prefix/share/applications/$desktop_name")
  done
  for icon_name in leapk.svg kleap.svg active-window-outline.svg; do
    system_files+=("$prefix/share/icons/hicolor/scalable/apps/$icon_name")
  done
done

installed_system_files=()
for file in "${system_files[@]}"; do
  if [[ -e "$file" ]]; then
    installed_system_files+=("$file")
  fi
done

if ((${#installed_system_files[@]})); then
  run_privileged rm -f -- "${installed_system_files[@]}"
fi

echo "[3/5] Removing user-level plugins, binaries, desktop launchers, and configs…"
# User binaries and symlinks in ~/.local/bin
rm -f "$HOME/.local/bin/leapk" \
      "$HOME/.local/bin/leapk-tray" \
      "$HOME/.local/bin/leapk-settings" \
      "$HOME/.local/bin/leapk-marker-manager" \
      "$HOME/.local/bin/leapk-workspace-manager" \
      "$HOME/.local/bin/leapk-custom-marker" \
      "$HOME/.local/bin/leapk-colorpicker" \
      "$HOME/.local/bin/kleap" \
      "$HOME/.local/bin/kleap-tray" \
      "$HOME/.local/bin/kleap-settings" \
      "$HOME/.local/bin/kleap-marker-manager" \
      "$HOME/.local/bin/kleap-workspace-manager" \
      "$HOME/.local/bin/kleap-custom-marker" \
      "$HOME/.local/bin/kleap-colorpicker" \
      "$HOME/.local/bin/active-window-outline-settings" \
      "$HOME/.local/bin/active-window-outline-tray" \
      "$HOME/.local/bin/active-window-outline-marker-manager" \
      "$HOME/.local/bin/active-window-outline-workspace-manager" \
      "$HOME/.local/bin/active-window-outline-colorpicker"

# User libexec
rm -rf "$HOME/.local/libexec/leapk-"* \
       "$HOME/.local/libexec/kleap-"* \
       "$HOME/.local/libexec/active-window-outline-"* \
       "$HOME/.local/lib/x86_64-linux-gnu/libexec/leapk-"* \
       "$HOME/.local/lib/x86_64-linux-gnu/libexec/kleap-"* \
       "$HOME/.local/lib/x86_64-linux-gnu/libexec/active-window-outline-"* 2>/dev/null || true

# User KWin plugins
rm -f "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/leapk.so" \
      "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/configs/kwin_leapk_config.so" \
      "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin-x11/effects/plugins/leapk.so" \
      "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin-x11/effects/configs/kwin_leapk_config.so" \
      "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/leapk.so" \
      "$HOME/.local/lib/qt6/plugins/kwin/effects/configs/kwin_leapk_config.so" \
      "$HOME/.local/lib64/qt6/plugins/kwin/effects/plugins/leapk.so" \
      "$HOME/.local/lib64/qt6/plugins/kwin/effects/configs/kwin_leapk_config.so" \
      "$HOME/.local/share/kwin/effects/plugins/leapk.so" \
      "$HOME/.local/share/kwin/effects/configs/kwin_leapk_config.so" \
      "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/active-window-outline.so" \
      "$HOME/.local/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/configs/kwin_active-window-outline_config.so" \
      "$HOME/.local/share/kwin/effects/plugins/active-window-outline.so" \
      "$HOME/.local/share/kwin/effects/configs/kwin_active-window-outline_config.so"

# User desktop files, autostart, icons, share
rm -f "$HOME/.local/share/applications/leapk-settings.desktop" \
      "$HOME/.local/share/applications/leapk-tray.desktop" \
      "$HOME/.local/share/applications/leapk-marker-manager.desktop" \
      "$HOME/.local/share/applications/kleap-settings.desktop" \
      "$HOME/.local/share/applications/kleap-tray.desktop" \
      "$HOME/.local/share/applications/kleap-marker-manager.desktop" \
      "$HOME/.local/share/applications/active-window-outline-settings.desktop" \
      "$HOME/.local/share/applications/active-window-outline-tray.desktop" \
      "$HOME/.local/share/applications/active-window-outline-marker-manager.desktop" \
      "$HOME/.config/autostart/leapk-tray.desktop" \
      "$HOME/.config/autostart/kleap-tray.desktop" \
      "$HOME/.config/autostart/active-window-outline-tray.desktop" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/leapk.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/kleap.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/active-window-outline.svg"
rm -rf "$HOME/.local/share/leapk" 2>/dev/null || true

# User configuration and cache directories
rm -rf "$HOME/.config/leapk" \
       "$HOME/.config/kleap" \
       "$HOME/.config/active-window-outline" \
       "$HOME/.cache/leapk_sounds" \
       "$HOME/.cache/leapk" 2>/dev/null || true

echo "[4/5] Removing effect settings and system shortcuts…"
for key in UseAccentColor AccentOpacity CustomColor EnsurePerimeterContrast BorderWidth OutlineGap \
           AmbilightEnabled AmbilightMode AmbilightOpacity AmbilightFrequency AmbilightDuration AmbilightSaturationBoost AmbilightSaturationLeveling AmbilightFallbackStrength AmbilightMinBrightness AmbilightMaxBrightness AmbilightEnsureContrast \
           BorderRadius FadeCorners DrawInside SmoothEdges GlowEnabled GlowRadius HideOnFullScreen TrayIconEnabled ActivePreset \
           ColorCycleEnabled ColorList ColorTransitionDuration ColorCycleOrder ColorCycleEasing \
           BreatheEnabled BreatheSpeed BreatheIntensity \
           GlitchEnabled GlitchSpeed GlitchIntensity GlitchAnomalyEnabled GlitchFrequency \
           NeonFlickerEnabled NeonFlickerInterval NeonFlickerDuration NeonFlickerIntensity \
           FlashEnabled FlashInterval FlashDuration \
           CustomShaderEnabled ActiveCustomShaderId ActiveThemeId ActiveThemeParams \
           AudioTrackingEnabled AudioHudIndicatorStyle AudioHudPulseEnabled AudioHudPulseSpeed AudioHudPulseIntensity AudioHudIndicatorColor AudioHudIndicatorColorAuto \
           AudioWindowIndicatorStyle AudioWindowPulseEnabled AudioWindowPulseSpeed AudioWindowPulseIntensity AudioWindowIndicatorScale AudioShaderRuleEnabled AudioShaderId \
           FadeDuration MenuHideDelay MenuShowDelay WindowHideDelay WindowShowDelay \
           MarkDefaultColor MarkSmartAdaptive MarkRandomColor StylePrecedence MarkInactiveDashGap MarkInactiveWidthReduction MarkInactiveDashed MarkInactiveDashSmooth MarkShowLetterBadges BadgesAlwaysVisible BadgeDisplayMode BadgeWindowBadgesEnabled BadgePosition BadgeHudPosition BadgeSize BadgeMargin BadgeShowIcon BadgeShowThumbnail BadgeWindowShowIcon BadgeWindowShowThumbnail BadgeWindowIconSize BadgeWindowThumbnailWidth BadgeWindowThumbnailHeight BadgeWindowLetterFontSize BadgeWindowLabelFontSize BadgeHudShowIcon BadgeHudShowThumbnail BadgeHudIconSize BadgeHudThumbnailWidth BadgeHudThumbnailHeight BadgeHudLetterFontSize BadgeHudCaptionFontSize BadgeHudLocationFontSize BadgeHudNavFontSize BadgeHudHintsFontSize BadgeHudShowHints BadgeHudIncludeAll BadgeHudGroupWindows BadgeHudGroupBorderWidth BadgeHudGroupFontSize BadgeHudGroupCornerRadius BadgeHudGroupShaderEnabled BadgeLivePreviews BadgeTimeout JumpRecentKey BadgeJumpSize BadgeHudSize BadgeIconSize BadgeThumbnailWidth BadgeThumbnailHeight BadgeWindowThumbnailSize BadgeHudThumbnailSize BadgeCornerRadius BadgeOpacity BadgeHudMarkColorMode BadgeDefaultColor BadgeLetterColorAuto BadgeLetterColor BadgeLabelColorAuto BadgeLabelUnmarkedColor BadgeCaptionColorAuto BadgeCaptionColor BadgeLocationColorAuto BadgeLocationColor BadgeLetterFont BadgeLabelFont BadgeCaptionFont BadgeLocationFont BadgeLetterFontSize BadgeLabelFontSize BadgeCaptionFontSize BadgeLocationFontSize JumpTargets CursorWarpMode SoundFeedbackEnabled SoundFeedbackVolume \
           HighlightRecentWindow RecentWindowsCount RecentWindowColorMode RecentWindowCustomColor RecentWindowOpacity RecentWindowHueShift RecentWindowSaturation RecentWindowWidthReduction RecentWindowDashed RecentWindowGapGrowth RecentWindowDashSmooth; do
  kwriteconfig6 --file kwinrc --group "$config_group" --key "$key" --delete '' 2>/dev/null || true
  kwriteconfig6 --file kwinrc --group "Effect-active-window-outline" --key "$key" --delete '' 2>/dev/null || true
done
kwriteconfig6 --file kwinrc --group Plugins --key "leapkEnabled" --delete '' 2>/dev/null || true
kwriteconfig6 --file kwinrc --group Plugins --key "active-window-outlineEnabled" --delete '' 2>/dev/null || true

# Clean up all global shortcuts registered under kwin
global_shortcuts=(
  "ToggleActiveWindowOutline"
  "MarkActiveWindow"
  "MarkActiveWindowWithColor"
  "OpenWorkspaceManager"
  "OpenMarkerManager"
  "ToggleHudGrouping"
  "TogglePinActiveWindow"
  "JumpToWindow"
  "JumpToAudioWindow"
  "JumpToSearchWindow"
  "RecentWindow"
  "PrevRecentWindow"
  "NextRecentWindow"
  "ToggleTilingPresets"
  "ToggleTilingEdit"
)
for sc in "${global_shortcuts[@]}"; do
  kwriteconfig6 --file kglobalshortcutsrc --group kwin --key "$sc" --delete '' 2>/dev/null || true
done

# Reload KGlobalAccel to free shortcuts immediately
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KGlobalAccel /KGlobalAccel org.kde.KGlobalAccel.reloadConfig >/dev/null 2>&1 || true
fi
if command -v dbus-send >/dev/null 2>&1; then
  dbus-send --session --dest=org.kde.KGlobalAccel --type=method_call /KGlobalAccel org.kde.KGlobalAccel.reloadConfig >/dev/null 2>&1 || true
fi

echo "[5/5] Refreshing KDE…"
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
  if [[ -d /usr/share/applications ]] && command -v sudo >/dev/null 2>&1; then
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
if command -v dbus-send >/dev/null 2>&1; then
  dbus-send --session --dest=org.kde.KWin --type=method_call /KWin org.kde.KWin.reconfigure >/dev/null 2>&1 || true
fi

echo
echo "Done. LeapK has been completely uninstalled and removed."
echo "Build dependencies and any local git repository checkouts were left untouched."
