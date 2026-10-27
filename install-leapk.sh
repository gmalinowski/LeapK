#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: bash install-leapk.sh [--skip-deps] [--dry-run] [-j|--jobs N]

Build and install the KWin Wayland effect for the KWin version on this system.
Dependencies are installed automatically on Debian/Ubuntu, Fedora, Arch,
and openSUSE families. Other distributions can use --skip-deps after installing
the development dependencies listed in the README.

  --skip-deps  Do not invoke a package manager.
  --dry-run    Show the detected dependency command without changing the system.
  --help       Show this message.
EOF
}

skip_deps=false
dry_run=false
parallel_jobs="${JOBS:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-deps) skip_deps=true; shift ;;
    --dry-run) dry_run=true; shift ;;
    -j|--jobs)
      if [[ $# -lt 2 ]]; then
        echo "Option $1 requires a numeric argument." >&2; exit 2
      fi
      parallel_jobs="$2"
      shift 2
      ;;
    -j*|--jobs=*)
      parallel_jobs="${1#*=}"
      if [[ "$1" == -j* && "$1" != --jobs=* ]]; then
        parallel_jobs="${1#-j}"
      fi
      shift
      ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ -z "$parallel_jobs" || ! "$parallel_jobs" =~ ^[0-9]+$ || "$parallel_jobs" -lt 1 ]]; then
  cpus="$(nproc 2>/dev/null || echo 2)"
  mem_kb="$(awk '/MemAvailable/ {print $2}' /proc/meminfo 2>/dev/null || true)"
  if [[ -z "$mem_kb" ]]; then
    mem_kb="$(awk '/MemTotal/ {print $2}' /proc/meminfo 2>/dev/null || true)"
  fi

  # Heavy C++20 / Qt6 / KWin headers require ~1.5 - 2.0 GB RAM per parallel compiler job.
  mem_jobs=2
  if [[ -n "$mem_kb" && "$mem_kb" -gt 0 ]]; then
    mem_gb=$(( mem_kb / 1024 / 1024 ))
    mem_jobs=$(( mem_gb / 2 ))
  fi
  (( mem_jobs < 1 )) && mem_jobs=1

  safe_jobs=$cpus
  (( safe_jobs > mem_jobs )) && safe_jobs=$mem_jobs
  # Cap at reasonable max (4) to prevent memory spikes on zero-swap or multi-core laptops
  (( safe_jobs > 4 )) && safe_jobs=4
  (( safe_jobs < 1 )) && safe_jobs=1
  parallel_jobs="$safe_jobs"
fi

if (( EUID == 0 )); then
  echo "Run this installer as your normal desktop user, not as root." >&2
  exit 1
fi

project_dir="$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
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
echo "LeapK v$version"
echo

if [[ -r /etc/os-release ]]; then
  # shellcheck disable=SC1091
  source /etc/os-release
fi

packages=()
package_manager=""
case "${ID:-}" in
  debian|ubuntu) package_manager=apt ;;
  fedora|rhel) package_manager=dnf ;;
  arch|archlinux|cachyos) package_manager=pacman ;;
  opensuse*|suse|sles) package_manager=zypper ;;
esac
if [[ -z "$package_manager" ]]; then
  case " ${ID_LIKE:-} " in
    *" debian "*|*" ubuntu "*) package_manager=apt ;;
    *" fedora "*|*" rhel "*) package_manager=dnf ;;
    *" arch "*|*" archlinux "*) package_manager=pacman ;;
    *" opensuse "*|*" suse "*) package_manager=zypper ;;
  esac
fi

case "$package_manager" in
  apt)
    packages=(build-essential cmake extra-cmake-modules pkg-config
      qt6-base-dev qt6-declarative-dev qt6-tools-dev-tools
      libkf6coreaddons-dev libkf6config-dev libkf6configwidgets-dev
      libkf6kcmutils-dev libkf6globalaccel-dev libvulkan-dev libdrm-dev kwin-wayland kwin-dev)
    ;;
  dnf)
    packages=(gcc-c++ cmake extra-cmake-modules pkgconf-pkg-config
      qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel
      kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kconfigwidgets-devel
      kf6-kcmutils-devel kf6-kglobalaccel-devel vulkan-headers libdrm-devel kwin-devel)
    ;;
  pacman)
    packages=(base-devel cmake extra-cmake-modules pkgconf
      qt6-base qt6-declarative qt6-tools
      kcoreaddons kconfig kconfigwidgets kcmutils kglobalaccel
      vulkan-headers libdrm kwin)
    ;;
  zypper)
    packages=(gcc-c++ cmake extra-cmake-modules pkgconf-pkg-config
      qt6-base-devel qt6-declarative-devel qt6-tools
      kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kconfigwidgets-devel
      kf6-kcmutils-devel kf6-kglobalaccel-devel vulkan-headers libdrm-devel kwin6-devel)
    ;;
esac

if [[ "$dry_run" == true ]]; then
  echo "Detected distribution: ${PRETTY_NAME:-unknown}"
  if [[ "$skip_deps" == true ]]; then
    echo "Dependency installation: skipped"
  elif [[ -n "$package_manager" ]]; then
    echo "Dependency command:"
    case "$package_manager" in
      apt) printf 'sudo apt install';;
      dnf) printf 'sudo dnf install';;
      pacman) printf 'sudo pacman -S --needed';;
      zypper) printf 'sudo zypper install';;
    esac
    printf ' %q' "${packages[@]}"
    printf '\n'
  else
    echo "Dependency installation: manual (unsupported package manager)"
  fi
  echo "Build backend: KWin Wayland"
  echo "Installation: system Qt plugin directory (requires sudo or doas)"
  exit 0
fi

run_privileged() {
  if command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  elif command -v doas >/dev/null 2>&1; then
    doas "$@"
  else
    echo "This installation needs sudo or doas for system packages and plugin files." >&2
    exit 1
  fi
}

echo "[1/5] Checking build dependencies…"
if [[ "$skip_deps" == false && -n "$package_manager" ]]; then
  if ! command -v "$package_manager" >/dev/null 2>&1; then
    echo "Expected package manager '$package_manager' was not found." >&2
    exit 1
  fi
  case "$package_manager" in
    apt) run_privileged apt install "${packages[@]}" ;;
    dnf) run_privileged dnf install "${packages[@]}" ;;
    pacman) run_privileged pacman -S --needed "${packages[@]}" ;;
    zypper) run_privileged zypper install "${packages[@]}" ;;
  esac
elif [[ "$skip_deps" == false ]]; then
  echo "No automatic package list for ${PRETTY_NAME:-this distribution}."
  echo "Continuing with the installed toolchain; CMake will report missing dependencies."
fi

for command in cmake kreadconfig6 kwriteconfig6; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required tool '$command' is missing. Install the matching Qt 6/KDE build package." >&2
    exit 1
  fi
done

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

plugin_info_tool=""
for candidate in qtplugininfo6 qtplugininfo-qt6; do
  if command -v "$candidate" >/dev/null 2>&1; then
    plugin_info_tool="$candidate"
    break
  fi
done
if [[ -z "$plugin_info_tool" ]]; then
  echo "Qt 6 plugin inspector is missing (qtplugininfo6 or qtplugininfo-qt6)." >&2
  exit 1
fi

plugin_dir="$(qt_plugin_dir || true)"
if [[ -z "$plugin_dir" || "$plugin_dir" != /* ]]; then
  echo "Unable to determine the Qt 6 plugin directory (qtpaths6 or qmake6)." >&2
  exit 1
fi
installed_plugin="$plugin_dir/kwin/effects/plugins/leapk.so"
if [[ ! -e "$installed_plugin" ]]; then
  installed_plugin="$plugin_dir/kwin/effects/plugins/active-window-outline.so"
fi
updating_existing=false
if [[ -e "$installed_plugin" ]]; then
  updating_existing=true
fi

build_dir="$(mktemp -d /tmp/leapk-build.XXXXXX)"
trap 'cmake -E remove_directory "$build_dir"' EXIT

echo "[2/5] Building LeapK (using $parallel_jobs parallel jobs)…"
cmake -S "$project_dir" -B "$build_dir" \
  -DKWIN_BACKEND=WAYLAND \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel "$parallel_jobs"

effect_binary="$build_dir/lib/kwin/effects/plugins/leapk.so"
plugin_info="$("$plugin_info_tool" "$effect_binary" 2>&1 || true)"
if [[ "$plugin_info" != *'org.kde.kwin.EffectPluginFactory'* \
   || "$plugin_info" != *'"Id": "leapk"'* \
   || "$plugin_info" != *'"Version": "'"$version"'"'* ]]; then
  echo "The compiled library does not contain valid KWin effect metadata." >&2
  echo "$plugin_info" >&2
  exit 1
fi

echo "[3/5] Unloading the previous version…"
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    active-window-outline >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    leapk >/dev/null 2>&1 || true
fi

echo "[4/5] Installing the KWin effect…"
run_privileged cmake --install "$build_dir"

# Clean up legacy system launchers from earlier versions
run_privileged rm -f /usr/share/applications/kleap-settings.desktop \
                     /usr/share/applications/kleap-tray.desktop \
                     /usr/share/applications/active-window-outline-settings.desktop \
                     /usr/share/applications/active-window-outline-tray.desktop \
                     /usr/local/share/applications/kleap-settings.desktop \
                     /usr/local/share/applications/kleap-tray.desktop \
                     /usr/local/share/applications/active-window-outline-settings.desktop \
                     /usr/local/share/applications/active-window-outline-tray.desktop || true

# Clean up legacy user-level launchers and autostarts
rm -f "$HOME/.local/share/applications/kleap-settings.desktop" \
      "$HOME/.local/share/applications/kleap-tray.desktop" \
      "$HOME/.local/share/applications/active-window-outline-settings.desktop" \
      "$HOME/.local/share/applications/active-window-outline-tray.desktop" \
      "$HOME/.config/autostart/kleap-tray.desktop" \
      "$HOME/.config/autostart/active-window-outline-tray.desktop"

# Install user-level desktop launchers and icons
mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/scalable/apps"
cp "$project_dir/leapk-settings.desktop" "$HOME/.local/share/applications/"
cp "$project_dir/leapk-tray.desktop" "$HOME/.local/share/applications/"
cp "$project_dir/leapk.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/leapk.svg"

# Install/update user binaries in ~/.local/bin if present
if [[ -d "$HOME/.local/bin" ]]; then
  cp -f "$build_dir/leapk-colorpicker" "$HOME/.local/bin/leapk-colorpicker" 2>/dev/null || true
  cp -f "$build_dir/leapk-tray" "$HOME/.local/bin/leapk-tray" 2>/dev/null || true
  cp -f "$build_dir/leapk-settings" "$HOME/.local/bin/leapk-settings" 2>/dev/null || true
  (cd "$HOME/.local/bin" && ln -sf leapk-colorpicker kleap-colorpicker && ln -sf leapk-colorpicker active-window-outline-colorpicker && ln -sf leapk-tray kleap-tray && ln -sf leapk-tray active-window-outline-tray && ln -sf leapk-settings kleap-settings && ln -sf leapk-settings active-window-outline-settings && ln -sf leapk-settings leapk) 2>/dev/null || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
fi
if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
fi

echo "[5/5] Enabling the effect…"
kwriteconfig6 --file kwinrc --group Plugins \
  --key leapkEnabled true
kwriteconfig6 --file kwinrc --group Plugins \
  --key active-window-outlineEnabled false

set_default() {
  local key="$1"
  local value="$2"
  local current
  current="$(kreadconfig6 --file kwinrc --group Effect-leapk \
    --key "$key" --default '')"
  if [[ -z "$current" ]]; then
    current="$(kreadconfig6 --file kwinrc --group Effect-active-window-outline \
      --key "$key" --default '')"
  fi
  if [[ -z "$current" ]]; then
    kwriteconfig6 --file kwinrc --group Effect-leapk \
      --key "$key" "$value"
  else
    kwriteconfig6 --file kwinrc --group Effect-leapk \
      --key "$key" "$current"
  fi
}

set_default UseAccentColor true
set_default AccentOpacity 66
set_default CustomColor "61, 174, 233"
set_default EnsurePerimeterContrast true
set_default AmbilightEnabled false
set_default AmbilightMode 0
set_default AmbilightOpacity 100
set_default AmbilightFrequency 1.5
set_default AmbilightDuration 0.5
set_default AmbilightSaturationLeveling 65
set_default AmbilightFallbackStrength 50
set_default AmbilightMinBrightness 25
set_default AmbilightMaxBrightness 85
set_default AmbilightEnsureContrast true
set_default BorderWidth 2
set_default OutlineGap 0
set_default BorderRadius 6
set_default FadeCorners true
set_default DrawInside true
set_default SmoothEdges true
set_default GlowEnabled false
set_default GlowRadius 6
set_default HideOnFullScreen true
set_default TrayIconEnabled true
set_default ColorCycleEnabled false
set_default ColorList "#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0"
set_default ColorTransitionDuration 2.0
set_default ColorCycleOrder 0
set_default ColorCycleEasing 0
set_default BreatheEnabled false
set_default BreatheSpeed 0.5
set_default BreatheIntensity 40
set_default GlitchEnabled false
set_default GlitchSpeed 1.0
set_default GlitchIntensity 50
set_default GlitchAnomalyEnabled false
set_default GlitchFrequency 3
set_default NeonFlickerEnabled false
set_default NeonFlickerInterval 3.0
set_default NeonFlickerDuration 0.25
set_default NeonFlickerIntensity 30
set_default FlashEnabled false
set_default FlashInterval 4.0
set_default FlashDuration 0.15
set_default CustomShaderEnabled false
set_default ActiveCustomShaderId ""
set_default FadeDuration 200
set_default MenuHideDelay 500
set_default MenuShowDelay 500
set_default WindowHideDelay 0
set_default WindowShowDelay 0
set_default HighlightRecentWindow false
set_default RecentWindowsCount 1
set_default RecentWindowColorMode 0
set_default RecentWindowCustomColor "127, 140, 141"
set_default RecentWindowOpacity 20
set_default RecentWindowHueShift 0
set_default RecentWindowSaturation 0
set_default RecentWindowWidthReduction 0
set_default RecentWindowDashed false
set_default RecentWindowGapGrowth 6
set_default RecentWindowDashSmooth false
set_default MarkDefaultColor "231, 76, 60"
set_default MarkSmartAdaptive true
set_default MarkRandomColor false
set_default StylePrecedence "Marked,Active,Recent"
set_default MarkInactiveWidthReduction 1
set_default MarkInactiveDashed true
set_default MarkInactiveDashGap 6
set_default MarkInactiveDashSmooth true
set_default MarkShowLetterBadges true
set_default BadgesAlwaysVisible false
set_default BadgeDisplayMode 0
set_default BadgeWindowBadgesEnabled true
set_default BadgePosition 1
set_default BadgeHudPosition 0
set_default BadgeHudIncludeAll false
set_default BadgeLivePreviews false
set_default BadgeTimeout 5
set_default JumpRecentKey "Space"
set_default BadgeSize 28
set_default BadgeMargin 0
set_default BadgeShowIcon false
set_default BadgeShowThumbnail false
set_default BadgeWindowShowIcon false
set_default BadgeWindowShowThumbnail false
set_default BadgeWindowIconSize 20
set_default BadgeWindowThumbnailSize 56
set_default BadgeWindowThumbnailWidth 56
set_default BadgeWindowThumbnailHeight 32
set_default BadgeWindowLetterFontSize 0
set_default BadgeWindowLabelFontSize 0
set_default BadgeHudShowIcon true
set_default BadgeHudShowThumbnail true
set_default BadgeHudIconSize 24
set_default BadgeHudThumbnailSize 80
set_default BadgeHudThumbnailWidth 80
set_default BadgeHudThumbnailHeight 48
set_default BadgeHudLetterFontSize 0
set_default BadgeHudCaptionFontSize 0
set_default BadgeHudLocationFontSize 0
set_default BadgeJumpSize 34
set_default BadgeHudSize 36
set_default BadgeIconSize 20
set_default BadgeThumbnailWidth 56
set_default BadgeThumbnailHeight 32
set_default BadgeCornerRadius 6
set_default BadgeOpacity 92
set_default BadgeHudMarkColorMode 0
set_default BadgeDefaultColor "#232629"
set_default BadgeLetterColorAuto true
set_default BadgeLetterColor "#ffffff"
set_default BadgeLabelColorAuto true
set_default BadgeLabelUnmarkedColor "#f1c40f"
set_default BadgeCaptionColorAuto true
set_default BadgeCaptionColor "#dce1e6"
set_default BadgeLocationColorAuto true
set_default BadgeLocationColor "#f1c40f"
set_default BadgeLetterFont ""
set_default BadgeLabelFont ""
set_default BadgeCaptionFont ""
set_default BadgeLocationFont ""
set_default BadgeLetterFontSize 0
set_default BadgeLabelFontSize 0
set_default BadgeCaptionFontSize 0
set_default BadgeLocationFontSize 0
set_default JumpTargets 0

if command -v kreadconfig6 >/dev/null 2>&1 && command -v kwriteconfig6 >/dev/null 2>&1; then
  migrate_shortcut() {
    local key="$1"
    local def_key="$2"
    local label="$3"
    local current
    current="$(kreadconfig6 --file kglobalshortcutsrc --group kwin --key "$key" 2>/dev/null || true)"
    if [[ -z "$current" ]]; then
      kwriteconfig6 --file kglobalshortcutsrc --group kwin --key "$key" "$def_key,$def_key,$label"
      return
    fi
    local active_keys="${current%%,*}"
    [[ -z "$active_keys" ]] && active_keys="$def_key"
    if [[ "$key" == "MarkActiveWindow" && "$active_keys" == "Meta+Ctrl+M" ]]; then
      active_keys="$def_key"
    elif [[ "$key" == "MarkActiveWindowWithColor" && "$active_keys" == *"Meta+Ctrl+Shift+M"* ]]; then
      active_keys="${active_keys//Meta+Ctrl+Shift+M/Meta+Ctrl+Shift+Space}"
    elif [[ "$key" == "JumpToWindow" && "$active_keys" == *"Meta+Alt+Space"* ]]; then
      active_keys="$def_key"
    elif [[ "$key" == "PrevRecentWindow" && "$active_keys" == *"Meta+Alt+["* ]]; then
      active_keys="$def_key"
    elif [[ "$key" == "NextRecentWindow" && "$active_keys" == *"Meta+Alt+]"* ]]; then
      active_keys="$def_key"
    fi
    kwriteconfig6 --file kglobalshortcutsrc --group kwin --key "$key" "$active_keys,$def_key,$label"
  }

  migrate_shortcut "ToggleActiveWindowOutline" "Meta+Ctrl+O" "LeapK: Toggle Window Highlight"
  migrate_shortcut "MarkActiveWindow" "Meta+Ctrl+Space" "LeapK: Toggle Mark Window"
  migrate_shortcut "MarkActiveWindowWithColor" "Meta+Ctrl+Shift+Space" "LeapK: Mark Window with Color Picker"
  migrate_shortcut "JumpToWindow" "Meta+Space" "LeapK: Jump to Window"
  migrate_shortcut "RecentWindow" "" "LeapK: Switch to Recent Window"
  migrate_shortcut "PrevRecentWindow" "Meta+Ctrl+[" "LeapK: Previous Recent Window"
  migrate_shortcut "NextRecentWindow" "Meta+Ctrl+]" "LeapK: Next Recent Window"
fi

if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    active-window-outline >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    leapk >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
    leapk >/dev/null 2>&1 || true
fi

rm -f "$HOME/.config/autostart/active-window-outline-tray.desktop" "$HOME/.config/autostart/kleap-tray.desktop"
mkdir -p "$HOME/.config/autostart"
if [[ -f "$project_dir/leapk-tray.desktop" ]]; then
  cp "$project_dir/leapk-tray.desktop" "$HOME/.config/autostart/"
fi

tray_enabled="$(kreadconfig6 --file kwinrc --group Effect-leapk --key TrayIconEnabled --default '' 2>/dev/null || true)"
if [[ -z "$tray_enabled" ]]; then
  tray_enabled="$(kreadconfig6 --file kwinrc --group Effect-active-window-outline --key TrayIconEnabled --default true 2>/dev/null || echo true)"
fi
if [[ "$tray_enabled" == "true" ]]; then
  if command -v leapk-tray >/dev/null 2>&1; then
    leapk-tray >/dev/null 2>&1 &
  elif command -v kleap-tray >/dev/null 2>&1; then
    kleap-tray >/dev/null 2>&1 &
  elif command -v active-window-outline-tray >/dev/null 2>&1; then
    active-window-outline-tray >/dev/null 2>&1 &
  elif [[ -x "$build_dir/bin/leapk-tray" ]]; then
    "$build_dir/bin/leapk-tray" >/dev/null 2>&1 &
  elif [[ -x "$build_dir/bin/kleap-tray" ]]; then
    "$build_dir/bin/kleap-tray" >/dev/null 2>&1 &
  elif [[ -x "$build_dir/bin/active-window-outline-tray" ]]; then
    "$build_dir/bin/active-window-outline-tray" >/dev/null 2>&1 &
  fi
fi

echo
echo "Done. LeapK v$version has been installed."
echo "Configure it in System Settings → Desktop Effects → LeapK (or via 'leapk-settings')."
echo "Toggle window highlight: Meta+Ctrl+O (search for 'LeapK' in Shortcuts)."
echo "Jump to window: Meta+Space (search for 'LeapK' in Shortcuts)."
echo "Mark window: Meta+Ctrl+Space (toggle mark), Meta+Ctrl+Shift+Space (mark with palette)."
if [[ "$updating_existing" == true ]]; then
  echo
  echo "IMPORTANT: This was an update of a native KWin plugin."
  echo "Log out and back in once so KWin loads the new library from disk."
else
  echo "If the effect does not appear immediately, log out and back in."
fi
