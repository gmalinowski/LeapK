# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses semantic versioning.

## [1.2.13] - 2026-10-20

### Added

- Add configurable option to automatically hide the active window outline and glow when the active window is fullscreen (enabled by default).

## [1.2.12] - 2026-10-20

### Added

- Global shortcut (default: `Meta+Ctrl+O`) to toggle the active window outline on or off via KGlobalAccel, configurable in System Settings → Shortcuts → KWin.

## [1.2.11] - 2026-10-19

### Fixed

- On Wayland with KWin 6.6+, defer the software cursor until after the outline
  in ordinary screen passes. Preserve KWin's existing hardware-cursor exclusions
  and native cursor rendering, instead of painting over the cursor or drawing
  it twice. Verified on CachyOS (KWin 6.8+).

### Added

- Cursor-filter regression tests for subtree exclusion, output isolation,
  early-return cleanup, and preservation of existing hardware-plane exclusions.

## [1.2.10] - 2026-10-19

### Fixed

- Clip the final screen-level outline to visible panels stacked above the
  active window. On Wayland, use Plasma's animated blur/contrast region when
  available so floating-panel margins do not cut the outline in the wrong
  place. Use the surface origin for mask coordinates and the panel's input
  region as a conservative fallback when its theme supplies no background mask.
- Use KWin 6.7+'s `SurfaceInterface::blurRegion()` instead of removed Wayland
  blur headers, retaining the older API for KWin 6.4–6.6.
- Recognize grabbed popups as blocking menus, including untyped Wayland
  `xdg_popup` windows and KWin's internal titlebar menu. Ungrabbed generic
  popups do not hide the outline merely because they are popups.
- Suppress the outline for input-accepting Wayland popups in the active
  window's transient tree, covering Firefox context menus that do not request
  a popup grab. Exclude explicit tooltips and input-transparent popups from
  this fallback.

### Added

- Read-only window/panel diagnostics with a loaded-build fingerprint and a
  capture script for reproducing context-menu and floating-panel issues.
- Optional popup-filter regression tests covering interactive menus, nested
  popups, tooltips, input-transparent surfaces, ownership, and deleted windows.

## [1.2.9] - 2026-10-19

### Fixed

- Add a KWin 6.4.x code path using its render-viewport clipping API and a
  custom glow vertex shader instead of the newer `ShaderTrait::Border`.
  Compatibility with KWin 6.4.x has not yet been tested on that version.
- Find Qt's plugin directory on Arch-based systems where `qtpaths6` is installed
  outside `PATH`; use `qmake6` as a fallback in both installer and uninstaller.

## [1.2.8] - 2026-10-19

### Fixed

- Build against newer KWin headers used by KDE neon: handle the boolean
  `paintScreen()` API, use a stable OpenGL context check, and accept the newer
  shader API without `GLShader::isValid()`.

## [1.2.7] - 2026-10-19

### Changed

- Add guided dependency installation for Debian/Ubuntu, Fedora, Arch, and
  openSUSE families, with `--dry-run` and `--skip-deps` for other systems.
- Use the new 2 px, 0 px offset, 6 px radius, inside-drawing defaults
  consistently in the effect, settings page, and installer. Existing user
  settings remain untouched on update.
- Make the uninstaller accept `doas` as well as `sudo` and remove all effect
  settings, including opacity, smoothing, and glow.

## [1.2.6] - 2026-10-19

### Changed

- New installations now default to the Plasma accent color at 66% opacity,
  a 4 px outside outline with 0 px offset and 5 px radius, smooth edges, and
  soft glow enabled at 6 px. Existing saved settings are preserved on update.

## [1.2.5] - 2026-10-19

### Added

- An accent-opacity setting (0–100%) for the outline and glow when "Use accent
  color" is enabled. Custom colors continue to use the alpha channel in the
  color picker.

## [1.2.4] - 2026-10-19

### Fixed

- Convert screen damage into render-target coordinates before applying OpenGL
  clipping. This fixes outline segments disappearing after the 1.2.3 flicker
  correction, including on scaled or rotated outputs.

### Changed

- Replace the four hard-edged glow bands with one continuous, subdued color
  gradient. The setting is now labeled "Soft glow".

## [1.2.3] - 2026-10-19

### Fixed

- Enable and restore OpenGL scissor clipping while drawing the outline. This
  prevents smooth-edge and glow layers from being redrawn outside the damaged
  region during partial screen updates, which caused visible flicker and a
  discolored fringe when hovering controls or typing.
- Round the outline repaint area outward so fractional window coordinates do
  not cut off the outermost glow pixels.

## [1.2.2] - 2026-10-19

### Fixed

- Restore KWin's previous OpenGL blend state after drawing the outline, so
  hover highlights and partial repaints cannot inherit the glow blend mode.
- Render the neon halo with normal alpha blending, preserving the outline hue
  on light backgrounds instead of washing the halo out to white.

## [1.2.1] - 2026-10-19

### Fixed

- Prevented glow flicker while typing or moving the pointer by no longer
  treating generic toolkit popups and tooltips as context menus.
- Switched glow layers to additive blending and increased their falloff
  strength so the halo preserves the outline color instead of appearing gray.

## [1.2.0] - 2026-10-19

### Added

- Optional edge smoothing using translucent feather bands on both sides of the
  solid outline.
- Optional neon glow rendered as four progressively fading layers, with a
  configurable glow size.

### Changed

- New installations now default to a 4 px outside outline with 0 px offset
  and a 5 px corner radius.

## [1.1.6] - 2026-10-19

### Changed

- Simplified popup handling: the active-window outline is temporarily hidden
  whenever KWin reports a visible context menu, submenu, dropdown, combo-box
  popup, tooltip, or Plasma panel popup. It returns when the popup closes.
- Removed the experimental composite outline around application menus because
  toolkit surface geometry could still leave the window outline above them.
- The installer now builds in a temporary directory and cleans it on exit, so
  installing the effect no longer leaves binaries inside the source tree.

## [1.1.5] - 2026-10-19

### Fixed

- Restored the generated Qt plugin metadata includes after the source files
  were renamed for the public repository. Without them, version 1.1.4 built
  successfully but KWin could not discover either plugin.
- The installer now validates the compiled KWin plugin ID and version before
  replacing an installed copy.

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
