# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses semantic versioning.

## [1.4.2] - 2026-10-21

### Changed

- **Ambilight color mixing algorithm**: Switched from circular polar hue vector averaging to weighted additive RGB light mixing. In polar hue space, opposing colorful elements (e.g. syntax highlighting, diverse UI icons, charts) cancel each other's vectors out to near zero, causing erratic quadrant-flipping and jumping colors. Additive RGB blending harmoniously merges multiple colors into stable, pleasing composite accents (e.g. red + blue -> magenta, complementary pairs -> neutral slate).
- **Higher resolution sampling grid**: Increased Ambilight sampling resolution from 16×16 (256 samples) to 32×32 (1024 samples). The 4× denser sampling eliminates spatial aliasing on small text and micro-elements across large 1080p/4K windows while preserving sub-millisecond readback performance.

## [1.4.1] - 2026-10-21

### Fixed

- **Appearance tab layout alignment**: Fixed an issue in `activewindowoutline_config.ui` where row numbers in the Appearance form layout collided, causing an unlabeled spinbox with "px" to display errantly without a label.

### Added

- **Ambilight sampling frequency control**: Added a configurable sampling frequency slider (`AmbilightFrequency`, 0.1 to 10.0 Hz, default 1.0 Hz) with an informative notice explaining that higher frequencies react faster to dynamic video content but increase CPU utilization.
- **Ambilight opacity control**: Added an opacity slider (`AmbilightOpacity`, 0 to 100%, default 100%) to allow transparency in Ambilight mode.
- **Mutual exclusion with Color Sequence**: When Color Sequence is enabled, Appearance color settings (Accent color, Custom color) and the entire Ambilight group are automatically disabled / grayed out.

## [1.4.0] - 2026-10-21

### Added

- **Ambilight mode (Adaptive window color)**: Added dynamic outline color adaptation that extracts the dominant accent color from the active window's rendered content.
- **Vivaldi-inspired scoring algorithm**: Uses saturation-weighted circular hue averaging with luminance penalties for dark and white canvases, prioritizing colorful brand/UI accents over vast empty backgrounds. Includes fallback to contrast-clamped neutral tones for monochrome/grayscale windows.
- **Luminance clamping & Saturation boost**: Configurable minimum brightness (prevents outlines disappearing on pitch-black windows/terminals), maximum brightness (prevents blowing out on pure white documents), and saturation boost multiplier.
- **Throttled 1 Hz live sampling with fluid easing**: Samples the active window at 1 Hz (and immediately upon window activation/geometry changes) with zero perceptible GPU/CPU overhead. Color shifts are smoothly interpolated using cubic ease-in-out easing without stroboscopic jumping.
- **Configuration controls**: Added dedicated Ambilight controls in the Appearance settings tab for enabling the feature, adjusting transition duration, saturation boost, and brightness bounds.

## [1.3.6] - 2026-10-21

### Fixed

- **Alpha channel support in color cycling sequence**: Enabled the alpha / opacity slider in the color sequence table picker by passing `QColorDialog::ShowAlphaChannel`.
- **Transparency serialization**: Updated color list serialization and display to format semi-transparent colors as `#AARRGGBB` (`HexArgb`) while maintaining standard `#RRGGBB` format for opaque colors, preventing opacity values from being stripped upon saving.
- **Checkerboard swatches**: Rendered a crisp checkerboard backdrop for semi-transparent color items in the color sequence table so transparency is visually distinct from solid colors.

## [1.3.5] - 2026-10-20

### Fixed

- **Fade transition for switch hide**: Added smooth fade-out transition for the deactivated window when switching windows. Previously, the deactivated window's outline would abruptly snap to 0% opacity upon hiding or buffer delay expiration. `DelayedHideState` now smoothly steps the deactivated outline's opacity from 1.0 down to 0.0 over `FadeDuration` ms (either immediately if `WindowHideDelay == 0`, or following the buffer delay if `WindowHideDelay > 0`).

## [1.3.4] - 2026-10-20

### Fixed

- **Window switch hide delay with show delay**: Fixed an issue where the delayed hide window outline disappeared instantly when switching windows with both `WindowHideDelay > 0` and `WindowShowDelay > 0`. `isActive()` previously returned `false` while the window show delay timer was active, which caused KWin compositor to skip rendering the effect entirely during that interval.
- **Decoupled opacity for delayed hide windows**: Delayed hide windows are now rendered at full opacity independently of the newly active window's transition opacity.
- **Window switch repaint damage**: Scheduled explicit outline repaints for previously active windows when buffering window switch delays.

## [1.3.3] - 2026-10-20

### Fixed

- **Smooth fade-in interpolation after idle periods**: Fixed an issue where the animation timer's delta time (`dt`) evaluated against stale timestamps from when the outline was hidden, causing the outline to snap to 100% opacity in a single frame. Frame delta is now capped to 50 ms and resets cleanly when transitions begin.
- **Menu show buffer bypass**: Prevented `slotWindowActivated` from prematurely forcing the outline to 100% opacity when windows regain focus upon menu closure, ensuring that `MenuShowDelay` and `FadeDuration` are fully respected.
- **Popup window activation**: Explicitly ignore activation events from popup and context menu windows so the active outlined window identity is never corrupted.
- **Self-healing timer integration**: Routed state recovery through `updatePopupBlockingState()` so that asynchronous delay timers are reliably started.

## [1.3.2] - 2026-10-20

### Fixed

- **Critical popup blocking recovery bug**: In Wayland, when a context menu or popup closed, its surface was unmapped and marked deleted before the destruction signal was dispatched, causing `isBlockingPopup(w)` to evaluate to `false`. This prevented `updatePopupBlockingState()` from ever being called on menu closure, leaving `m_popupBlocksOutline` permanently active and causing the outline to disappear and never reappear.
- Fixed `slotWindowDeleted` and `slotWindowHiddenChanged` to unconditionally re-evaluate popup blocking state regardless of transient surface validity.
- Added automatic self-healing safeguard in `prepareScreenPaint` and `slotAnimationTick` that detects and recovers from any desynchronized popup state.

### Added

- **Smooth fade-in / fade-out transitions**: Outline smoothly animates its opacity when appearing and disappearing (controlled by `FadeDuration`, default: 200 ms) instead of abrupt snapping.
- **Configurable Transitions & Fading group**: Added `Fade transition duration` setting to the Behavior tab in System Settings KCM with instant (0 ms) option.
- **Automated test suite expansion**:
  - `outline-visibility-test`: automated verification of popup buffer delays, rapid menu hopping cancellation, smooth fade interpolation, instant delay/fade modes, self-healing recovery, and regression simulation for Wayland popup deletion.
  - `outline-color-transition-test`: color sequence syntax parsing, CSS easing functions, and RGBA blending.
  - `outline-shader-effects-test`: mathematical models and boundary conditions for breathe, neon, flash, and hologram effects.
  - `outline-geometry-test`: rounded rectangle strip VBO tessellation, radius clamping, and scaling.
  - `outline-config-sync-test`: automated verification that all KConfig schema entries exist in installer and uninstaller scripts.
- Refactored pure algorithmic modules into dedicated headers (`colortransition.h`, `shadereffects.h`, `geometryhelper.h`, `visibilitystate.h`).
- Added missing `CustomColor` default setting to `install-active-window-outline.sh`.

## [1.3.1] - 2026-10-20

### Added

- New **Behavior** tab in the configuration dialog with independent show/hide buffering controls:
  - **Menu hide delay**: configurable buffer before hiding the outline when context menus or popups open (default: 300 ms).
  - **Menu show delay**: configurable buffer before restoring the outline after menus close (default: 300 ms), completely eliminating outline flickering when navigating across menus and submenus.
  - **Switch hide delay**: configurable buffer before removing the outline from a deactivated window (default: 0 ms / instant).
  - **Switch show delay**: configurable buffer before drawing the outline on a newly focused window (default: 0 ms / instant).
- Popups stacked above the outlined window are clipped during any lingering hide buffer so the outline never renders over open menus.

## [1.3.0] - 2026-10-20

### Added

- Multi-color sequence / cycling with customizable hold durations per color, global transition time, sequential or random order, and CSS easing curves (Ease In Out, Linear, Ease In, Ease Out).
- Modular shader and animation effects with mix-and-match checkboxes and independent controls:
  - Breathe / Pulse effect (configurable speed and depth).
  - Futuristic Hologram shader effect with multi-frequency scanlines, rolling sweep beam, cyan ionization aura, and optional periodic glitch disturbances.
  - Neon Flicker effect with simulated voltage drops and configurable cycle interval and duration.
  - Flash / Strobe effect with configurable interval and burst duration.
- Tabbed KCM configuration UI (Appearance, Color Sequence, Shader Effects).
- Configurable option to automatically hide the active window outline and glow when the active window is fullscreen (enabled by default).
- High performance animation pipeline: 60 FPS repaints are isolated to the active window's bounding box; mesh geometry is cached with dynamic uniform updates; zero timer wakeups or CPU/GPU overhead when animation is inactive or windows are fullscreen/minimized.
- One-liner installer `--beta` flag to install the latest development build directly from the `main` branch.

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
