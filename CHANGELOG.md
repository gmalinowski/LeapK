# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses semantic versioning.

## [1.1.4] - 2026-10-19

### Fixed

- Context menus and nested submenus belonging to the active window are now
  included in the outlined shape. Overlapping outline segments are clipped so
  the border follows the exposed popup edges instead of crossing popup content.

## [1.1.3] - 2026-10-19

### Added

- Display the installed version in the graphical configuration window.
- Display the version at the beginning and end of installation.
- Add a top-level `VERSION` file as the canonical project version for builds
  and scripts.

## [1.1.2] - 2026-10-19

### Fixed

- Suppress the outline while application dropdown menus, context menus,
  combo-box popups, tooltips, or Plasma panel popups are visible, preventing
  the screen-level outline from being painted above popup content.

## [1.1.1] - 2026-10-19

### Added

- Automatic `kwinrc` monitoring as a fallback when KWin does not deliver a reconfiguration request.
- Runtime diagnostics for the loaded width, offset, radius, and placement mode.
- Update detection and an explicit logout notice in the installer.
- Uninstallation script.

### Changed

- Renamed the user-facing “Outline gap” setting to “Outline offset”.
- Existing settings are preserved when reinstalling or updating.
- The old effect is unloaded before its binary is replaced.

## [1.1.0] - 2026-10-19

### Added

- Configurable outline offset from 0 to 30 pixels.
- Inside and outside offset rendering.

## [1.0.0] - 2026-10-19

### Added

- Active-window tracking through KWin.
- Theme-independent OpenGL outline rendering.
- Plasma accent color and custom color support.
- Configurable outline width and corner radius.
- Graphical configuration module for Desktop Effects.
