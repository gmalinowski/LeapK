#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: bash install-active-window-outline.sh [--skip-deps] [--dry-run]

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
for arg in "$@"; do
  case "$arg" in
    --skip-deps) skip_deps=true ;;
    --dry-run) dry_run=true ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
  esac
done

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
echo "Active Window Outline v$version"
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
installed_plugin="$plugin_dir/kwin/effects/plugins/active-window-outline.so"
updating_existing=false
if [[ -e "$installed_plugin" ]]; then
  updating_existing=true
fi

build_dir="$(mktemp -d /tmp/active-window-outline-build.XXXXXX)"
trap 'cmake -E remove_directory "$build_dir"' EXIT

echo "[2/5] Building Active Window Outline…"
cmake -S "$project_dir" -B "$build_dir" \
  -DKWIN_BACKEND=WAYLAND \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel "$(nproc)"

effect_binary="$build_dir/lib/kwin/effects/plugins/active-window-outline.so"
plugin_info="$("$plugin_info_tool" "$effect_binary" 2>&1 || true)"
if [[ "$plugin_info" != *'org.kde.kwin.EffectPluginFactory'* \
   || "$plugin_info" != *'"Id": "active-window-outline"'* \
   || "$plugin_info" != *'"Version": "'"$version"'"'* ]]; then
  echo "The compiled library does not contain valid KWin effect metadata." >&2
  echo "$plugin_info" >&2
  exit 1
fi

echo "[3/5] Unloading the previous version…"
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    active-window-outline >/dev/null 2>&1 || true
fi

echo "[4/5] Installing the KWin effect…"
run_privileged cmake --install "$build_dir"
if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6
fi

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
set_default GlowEnabled true
set_default GlowRadius 6
set_default HideOnFullScreen true
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
set_default FadeDuration 200
set_default MenuHideDelay 300
set_default MenuShowDelay 300
set_default WindowHideDelay 0
set_default WindowShowDelay 0

if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect \
    active-window-outline >/dev/null 2>&1 || true
  qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
    active-window-outline >/dev/null 2>&1 || true
fi

echo
echo "Done. Active Window Outline v$version has been installed."
echo "Configure it in System Settings → Desktop Effects → Active Window Outline."
echo "Toggle outline shortcut: Meta+Ctrl+O (configurable in System Settings → Shortcuts → KWin)."
if [[ "$updating_existing" == true ]]; then
  echo
  echo "IMPORTANT: This was an update of a native KWin plugin."
  echo "Log out and back in once so KWin loads the new library from disk."
else
  echo "If the effect does not appear immediately, log out and back in."
fi
