# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses semantic versioning.

## [1.5.0] - 2026-10-25

### Added
- **Multi-Desktop Window Jumping & Offscreen HUD Cards (`Meta+Alt+Space`)**:
  - Modal Jump Mode now functions across all virtual desktops. Marked windows on other desktops and minimized windows retain their fixed letters and can be jumped to instantly from any virtual desktop.
  - Jumping to a window residing on a different desktop automatically switches virtual desktops (`effects->setCurrentDesktop`) and activates/unminimizes the window in one smooth step.
  - Floating Offscreen Jump Targets HUD: When Jump Mode is active and marked windows (or offscreen targets) exist on other desktops or in minimized state, an elegant, non-intrusive HUD bar appears at the bottom-center of the screen displaying each target's letter badge, desktop name/number (e.g. `Desktop 2`), and window title.
  - Expanded `JumpTargets` scope options in KCM settings:
    - `0`: Current desktop visible windows + marked windows from all desktops (default).
    - `1`: Marked windows only (all desktops).
    - `2`: All windows across all virtual desktops.
- **Randomized Mark Outline Color (`MarkRandomColor`)**:
  - Added a "Randomize color" checkbox next to Default Mark Color in the KCM settings.
  - When enabled, toggling a window mark automatically selects a unique, vibrant color from a curated high-contrast palette (with diverse HSV fallback) ensuring multiple marked windows each receive distinct, eye-catching outlines.
  - Automatically disables the static default color picker when randomize color is checked.
- **Multi-Desktop Virtual Desktop Focus History & Jump List (`Meta+Alt+[` / `Meta+Alt+]`)**:
  - Full-capacity 50-window focus history queue (`m_windowHistory`) independent of outline highlight limits.
  - Automatic virtual desktop switching (`effects->setCurrentDesktop(candidate->desktops().first())`) when jumping to windows residing on different virtual desktops.
  - Safe navigation state tracking that prevents transient null focus or intermediate desktop switch events from prematurely resetting the history sequence.
  - Seamless forward and backward traversal across any number of virtual desktops without scrambling or bouncing between the last two windows.
- **Window Occlusion Detection for Letter Badges (`isBadgeOccluded`)**:
  - Letter badges are now strictly hidden when occluded or covered by any overlapping window, dock/panel, or dialog higher in stacking order.
  - Badges never bleed through or draw over foreground windows.
  - In modal Jump Mode (`Meta+Alt+Space`), dynamically selects alternate visible corners/center if the primary position is covered; completely occluded windows do not display confusing floating badges.
  - Real-time full repaint when stacking order or virtual desktop changes (`stackingOrderChanged`, `desktopChanged`).

### Fixed
- **Immediate Previous Window Highlighting Order (`updateRecentWindowsFromHistory`)**:
  - Fixed an issue where the most recently deactivated window was skipped and outlines erroneously started from the 2nd most recent window instead of the 1st.
  - Active window assignment is now updated before computing recent window candidates from history, ensuring the immediate previous window is always at index 0 and highlighted as expected.
  - Added modular `OutlineHistory` helper and comprehensive unit test suite (`outline-history-test`).
- **Recent Window Highlight Count Limit (`RecentWindowsCount`)**:
  - Fixed an issue where all windows in history received outlines regardless of the configured limit.
  - The rendered recent window count now strictly honors `RecentWindowsCount` (e.g. 1 window only when set to 1), immediately erasing outlines and pruning cache when older windows cycle out of the recency limit.

## [1.4.30] - 2026-10-25

### Added
- **Configurable Letter Badge Position, Size, and Margin**:
  - **Customizable Badge Position (`BadgePosition`)**: Selectable in System Settings KCM with 9 positions:
    - `Top-Left`, `Top`, `Top-Right`
    - `Left`, `Center`, `Right`
    - `Bottom-Left`, `Bottom`, `Bottom-Right`
  - **Adjustable Badge Size (`BadgeSize`)**: Range from 16 px to 64 px (default: 28 px) with proportional width scaling, dynamic font size, and corner radii.
  - **Adjustable Edge Margin (`BadgeMargin`)**: Distance from window edges from 0 px to 100 px (default: 12 px) with automatic viewport boundary clamping.
  - **Unified & Intuitive Display Modes (`BadgeDisplayMode`)**:
    - `0`: *Only during Jump Mode (`Meta+Alt+Space`)* (clean desktop, badges only when navigating)
    - `1`: *Always visible on background windows* (badges continuously visible on non-active windows)
    - `2`: *Disabled* (completely disables letter badges)

### Fixed
- **Background Letter Badges Rendering & Visibility**:
  - Fixed an issue where continuous background letter badges did not appear because `paintScreen()` prematurely early-returned when no outlines were actively updating.
  - Fixed `isActive()` to return `true` when background badges are enabled, ensuring KWin compositor keeps the effect pipeline active.
  - Fixed window frame geometry change tracking and window focus change repaints so badges smoothly follow moving windows and instantly update when focus switches.

## [1.4.29] - 2026-10-24

### Added
- **Vim-like Window Marks with Letters & Instant Jump Mode**:
  - **Modal Jump Mode (`Meta+Alt+Space` / `KAWO: Jump to Window`)**:
    - Enters modal window jump navigation via native KWin compositor keyboard grabbing (`effects->grabKeyboard`).
    - Windows with assigned marks display their permanent letter on custom-colored pill badges.
    - Other candidate windows dynamically receive transient letters from a prioritized keyboard pool (`ASDFJKLQWERTYUIOP...`).
    - The currently active window is always excluded from displaying letter badges (`oprócz aktywnego okna`).
    - Instant window focus on single keypress (`effects->activateWindow`), automatically restoring minimized windows.
    - Safe cancel via `Esc` or 5-second automatic inactivity timeout.
    - Configurable in KCM: **Jump Targets** (dynamically showing current shortcut binding from KGlobalAccel), **Show Letter Badges on All Windows (except active window)**, and **Show Letter Badges Continuously** (disabled by default so badges only appear upon triggering Jump Mode).
  - **Quick & Detailed Window Marking**:
    - **`Meta+Ctrl+M` (`KAWO: Toggle Mark Window`)**: Quickly marks the active window with default color and automatically assigns the next available letter from the pool.
    - **`Meta+Ctrl+Shift+M` (`KAWO: Mark Window with Color Picker`)**: Opens dedicated native Qt dialog (`active-window-outline-colorpicker`) allowing setting both custom letter (`A-Z`, `0-9`) and custom mark color with alpha slider.
  - **Hardware-Accelerated Badge Rendering**: Crisp pill badge textures rendered via `QPainter` with antialiasing and drop shadows, cached as GPU textures and rendered with OpenGL blending.
- **Recent Windows History Navigation (Vim-like Jump List)**:
  - **`Meta+Alt+[` (`KAWO: Previous Recent Window`)**: Step backward through recent window focus history.
  - **`Meta+Alt+]` (`KAWO: Next Recent Window`)**: Step forward through recent window focus history.
  - History cursor snapshot prevents scrambling or modifying the window history order while actively cycling through windows; history commits after 1.5s of idle time.

## [1.4.28] - 2026-10-24

### Added
- **Outline Style Precedence System (`StylePrecedence`)**:
  - Replaced isolated priority checkboxes with a dedicated **Precedence** tab in the System Settings configuration module.
  - Interactive reorderable priority list (`QListWidget`) supporting both **Move Up / Move Down / Reset** buttons and smooth native drag-and-drop reordering.
  - Full definition priority semantics (**"w pełni definicji"**): the winning state in precedence completely dictates the window's outline appearance (color, width, dash pattern, gap, glow, and shader animations):
    - **Active > Marked**: Focused marked windows take the full active outline appearance (Accent, Custom, Ambilight, or Color Cycling, full border width, solid line, and active shaders).
    - **Marked > Active**: Focused marked windows retain their custom mark color and style without being overridden by active color or active shader effects.
    - **Marked > Recent**: Inactive marked windows in recent focus history retain their custom mark color and style, completely ignoring recent decay (saturation drop, hue shift, gap growth, and width reduction).
    - **Recent > Marked**: Inactive marked windows in recent focus history participate in recent window decay (taking recent color mode, opacity decay, width reduction, and dash gap growth).
  - Dynamic rule summary card that explains in plain language how the currently selected precedence order behaves.
  - New modular precedence parser and state resolver in `precedencehelper.h` accompanied by comprehensive unit tests (`outline-precedence`).

## [1.4.27] - 2026-10-23

### Added
- **Mark Window / Permanent Outline Frame (`m_markedWindows`)**:
  - Ability to pin a permanent outline frame to specific windows across their entire lifecycle.
  - **Immutable Window Color**: Marked windows strictly maintain their assigned mark color regardless of active outline color modes (System Accent, Custom Color, Ambilight, or Color Cycling), and never undergo hue shifting or saturation desaturation when aged or unfocused.
  - **Decoupled Inactive Appearance Settings**:
    - Marked windows have completely independent styling settings from Recent Windows:
      - **Alpha Channel Color Transparency**: Removed redundant `MarkInactiveOpacity` slider; opacity/transparency is now set directly via the color picker's alpha channel.
      - **Active Color Priority Option (`MarkActiveColorPriority`)**: Optional checkbox to let the active outline color take priority over the mark color while focused (across all active color modes: System Accent, Custom Color, Ambilight, or Color Cycling), gracefully returning to the custom mark color when unfocused.
      - `MarkInactiveWidthReduction` (0–10 px, default: 0 px)
      - `MarkInactiveDashed` (default: false)
      - `MarkInactiveDashGap` (1–40 px, default: 6 px) – configurable spacing between dashes when dashed outline is enabled.
      - `MarkInactiveDashSmooth` (default: false)
    - Inactive marked windows are always configurable and rendered according to their dedicated settings regardless of whether `HighlightRecentWindow` is enabled or disabled.
  - **Adaptive Hierarchy & Geometry**:
    - **Focused / Active State**: Marked windows adopt active outline properties (full border width, 100% opacity, solid border, glow and active shader animations).
    - **Inactive / Background State**: Marked windows adopt their dedicated inactive settings (width reduction, dashed pattern and gap) while retaining their permanent custom color and alpha transparency.
  - **Global Shortcuts (Prefixed with `KAWO:` for quick searching)**:
    - **`Meta+Ctrl+M` (`KAWO: Toggle Mark Window`)**: Toggle permanent mark outline on the active window using the default mark color (configurable, default: `#e74c3c` red).
    - **`Meta+Ctrl+Shift+M` (`KAWO: Mark Window with Color Picker`)**: Opens native color picker with full alpha channel support (`active-window-outline-colorpicker`, with `zenity` and non-modal `QColorDialog` fallbacks) to precisely choose any custom color and opacity for the active window without blocking the compositor event loop.
  - **KCM Configuration Section**: Dedicated Mark Window section in the System Settings KCM with controls for default color (with alpha channel), inactive width reduction, dashed styling, and dash gap.
- **Overview & Fullscreen Effect Clean Suppression**:
  - Automatically suppresses and clears outline frames when fullscreen interactive effects (such as Overview / `Super+W`, Present Windows, Desktop Grid, or transformed desktop switches) are active.
  - Prevents outline borders from remaining frozen in their original static screen positions across the miniaturized window thumbnails in the Overview grid.
  - Outlines cleanly disappear upon entering Overview and instantly reappear when exiting back to the workspace.

## [1.4.26] - 2026-10-23

### Added
- **Dashed Outline for Recent Windows (`RecentWindowDashed`)**:
  - Added option to render recent window outlines using a dashed line pattern instead of a solid line.
  - **Dynamic Age-Widening Gaps**: Gaps between dashes progressively increase for each older window in the focus history:
    - 1st recent window: ~12 px gap (6px base + 1x growth)
    - 2nd recent window: ~18 px gap (6px base + 2x growth)
    - 3rd recent window: ~24 px gap (6px base + 3x growth)
    - 4th recent window: ~30 px gap (6px base + 4x growth)
    - 5th recent window: ~36 px gap (6px base + 5x growth)
  - **Configurable Gap Growth (`RecentWindowGapGrowth`)**: Adjust how rapidly gaps expand per older window (0–30 px, default: 6 px), taking effect immediately from the first recent window. Setting to 0 px keeps gaps identical (6 px) across all recent windows.
  - **Smooth Dash Transitions (`RecentWindowDashSmooth`)**: Toggle smooth cubic Hermite (`smoothstep`) gradient fading between dashes and gaps for a softer, organic aesthetic instead of crisp cutoffs.
  - **Clearer Age-Decay UI Labels**: Clarified recent window opacity and saturation labels and tooltips in KCM settings to explicitly indicate that these controls specify target floors that fade progressively with window age.
  - **Seamless Loop & Subpixel Antialiasing**: Dash periods are adjusted to divide the window perimeter evenly with no truncated segments at corners, and dash boundaries are rendered with 1-pixel subpixel antialiasing.
  - **Matching Dashed Glow**: When glow is enabled, the soft glow shader adopts the exact same dash pattern and gaps.
  - **One-Liner Installer Update**: The `--beta` flag now directly targets the latest development build from the `main` branch.

## [1.4.25] - 2026-10-22

### Added
- **Recent / Previously Active Window Highlight (`HighlightRecentWindow`)**:
  - Added the ability to keep a subdued outline on recent previously active window(s) to track keyboard focus history and enhance multitasking / tiling workflows:
    - **Configurable History Depth (`RecentWindowsCount`)**: Highlight between 1 and 5 recent windows (default: 1).
    - **Linear Proportional Opacity Decay (`RecentWindowOpacity`)**: The user configures the outline opacity of the oldest tracked window (5% to 100%, default: 20%). Recent windows proportionally interpolate opacity between 100% (active window) and this minimum threshold (`1.0 - (step / N) * (1.0 - minOpacity)`).
    - **Proportional Saturation Decay / Grayscale Fade (`RecentWindowSaturation`)**: Each older window in history proportionally loses saturation down to the configured saturation of the oldest window (0% to 100%, default: 0% / full grayscale). Fades smoothly from full vibrant color on the active window into muted pastels and neutral monochrome on older windows.
    - **Color Mode Flexibility (`RecentWindowColorMode`)**:
      - **Match active window color**: Mirrors the active color mode (System Accent, Custom Color, or frozen per-window Ambilight colors).
      - **Custom static color (`RecentWindowCustomColor`)**: Explicitly assign a dedicated color for recent windows (e.g. slate gray, muted cyan).
    - **Ambilight Color Memory**: When Ambilight is active, recent windows retain the exact accent color that was sampled while they were active!
    - **Subdued & Calm**: Recent window outlines remain static without distracting shader animations (breathe, neon flicker, strobe).
    - **Occlusion-Aware**: Fully clipped against higher windows in the KWin stacking order; if windows overlap, active windows naturally occlude underlying recent outlines.
    - **Chromatic Recency Hue Shifting (`RecentWindowHueShift`)**: Optional setting to rotate the outline's hue angle (-180° to +180°) for each older window in the history (0° = no change), creating an intuitive visual color trail indicating focus progression.
    - **Stepwise Border Width Reduction (`RecentWindowWidthReduction`)**: Configure a step in pixels (0–10 px, default: 0 px / same width) by which each successive older window reduces its outline border thickness (`max(1, W - step * R)`). If the border reaches 1 px, it stops reducing and remains at 1 px, establishing an unmistakable physical thickness hierarchy.

### Fixed
- **Shader Effects & Hologram Suppression on Recent Windows**:
  - Fixed an issue where the Futuristic Hologram (Sci-Fi glitch) GPU fragment shader remained active on recent window outlines. Holographic scanlines, rolling sweep beam, micro-jitter, and anomaly surges are now strictly disabled on older windows.
  - Preserved static color for Color Cycling on recent windows by snapshotting the active cycle color upon focus departure, preventing older windows from actively cycling colors.
- **Recent Window Custom Color Hue Shifting Offset**:
  - When Custom Color mode is selected, the first recent window now strictly retains the exact configured custom color (0 shift), and chromatic hue rotation begins on the second recent window (window 2 shifts by 1x, window 3 by 2x). In Match Active mode, shifting begins from the first recent window relative to the active window.

## [1.4.24] - 2026-10-21

### Added
- **Global Perimeter Contrast Enforcement (`EnsurePerimeterContrast`)**:
  - Promoted "Ensure perimeter contrast" to a top-level setting in the Appearance tab, functioning globally across all color modes:
    - **Static Custom Color**: When a static color (e.g. green) is chosen, the engine monitors the perimeter. If the static color blends with the window border or desktop wallpaper, the outline dynamically adjusts lightness and shifts hue to maintain clear separation.
    - **KDE Accent Color**: When using system accent color (e.g. Breeze Blue), the outline adjusts if it clashes with the wallpaper (e.g. blue outline on blue wallpaper) or dark window frames.
    - **Ambilight (Adaptive Color)**: Continues to ensure Ambilight outlines never blend with window edges or wallpapers.
  - Backwards-compatible with older configs by automatically reading and synchronizing both `EnsurePerimeterContrast` and `AmbilightEnsureContrast`.

### Fixed
- **Plugin Metadata Restoration**:
  - Restored `#include "activewindowoutline.moc"` and added KWin factory macros to `CMAKE_AUTOMOC_MACRO_NAMES`, resolving the `No plug-in meta-data found` failure during installation while maintaining zero AutoMoc warnings.
- **Accurate Outside Perimeter Sampling & FBO Separation**:
  - Separated window content sampling and desktop wallpaper sampling into two dedicated framebuffers (`m_ambilightFbo` 32x32 and `m_outsideFbo` 32x8), eliminating coordinate inversion and row-offset overlap where window tabs were mistakenly sampled as outside wallpaper.
  - Expanded `expandedGeometryFor` repaint region by 18px when perimeter contrast is active, ensuring KWin compositing repaints the desktop wallpaper band around the active window so fresh wallpaper pixels are always available for contrast evaluation.
  - Used strict outward-bounding rectangles (`top - margin` to `top - 1`, `bottom + 1` to `bottom + margin`, `left - margin` to `left - 1`, `right + 1` to `right + margin`) preventing window border pixels from contaminating wallpaper samples.
- **Two-Sided Contrast Separation**:
  - Ensured hue shifting only triggers when the outline clashes with the background desktop wallpaper (`outHasColor`), preserving authentic window accent matches while preventing outlines from blending into the desktop wallpaper.
  - When the outline hue matches the window edge inside, enforced at least 0.28 lightness separation so the outline remains distinct and visible against the window frame.

## [1.4.23] - 2026-10-21

### Fixed
- **Compiler Warnings Cleanup (CachyOS / GCC)**:
  - Removed unused variable `unweightedLightnessSum` in `OutlineAmbilight::processDominantAccent` to eliminate `-Wunused-but-set-variable` warning on modern GCC / CachyOS toolchains.
  - Removed redundant `#include "activewindowoutline.moc"` at the end of `activewindowoutline.cpp` to eliminate the CMake automoc warning.

## [1.4.22] - 2026-10-21

### Added
- **Two-Sided Perimeter Contrast Enforcement (Window Edge + Desktop Wallpaper)**:
  - `Ensure Perimeter Contrast` now samples and enforces contrast in **both directions simultaneously**:
    1. **Outside the window**: Samples the 4 outer collar strips (16px surrounding `frameGeometry`) from the desktop scene, capturing the underlying desktop wallpaper or background windows.
    2. **Inside the window**: Samples the window's own inner perimeter (titlebar, borders, decorations).
  - If the outline's candidate or detected hue clashes with the desktop wallpaper outside (e.g. blue outline on a blue wallpaper, $|\Delta H| < 0.15$), the hue rotates away in $+0.25$ quadrant steps until it contrasts cleanly with **both** the wallpaper and the window edge.
  - Lightness is tuned to contrast against both sides:
    - If both sides are dark (e.g. dark terminal on dark wallpaper), outline is luminous neon ($L \ge 0.70$).
    - If both sides are light (e.g. light document on light wallpaper), outline is deep dark ($L \le 0.44$).
    - If one side is dark and one side is light, outline settles on a punchy midtone ($L \approx 0.56$) that clearly pops against both.

## [1.4.21] - 2026-10-21

### Fixed
- **Grayscale Completely Disabled in Smart Adaptive & Dominant Accent Modes**:
  - Black, white, silver, and shades of gray are now strictly excluded from Smart Adaptive (`Mode::Smart`) and Dominant Accent (`Mode::Dominant`) modes. An accent outline is always guaranteed to be a rich, distinct chromatic color ($S \ge 0.75$).
  - When window content contains no chromatic accents (e.g. black-and-white text documents, monochrome terminals, plain code editors), the algorithm resolves a vibrant chromatic fallback accent starting from the candidate hue (`previousHue` or KDE Breeze Blue $H = 0.555$).
- **Bounded Hue Shifting on Perimeter Clash (`Ensure Perimeter Contrast`)**:
  - When a window has no chromatic color to detect and `Ensure Perimeter Contrast` is enabled, the algorithm samples the window perimeter. If the candidate hue clashes with a colored window decoration or frame (e.g. green outline on a green window border, $|\Delta H| < 0.15$), the hue shifts in $+0.25$ quadrant steps (bounded to at most 4 attempts, preventing infinite loops) until high color contrast is achieved.
  - Lightness is automatically tuned for optimal separation ($L \le 0.44$ on light edges, $L \ge 0.68$ on dark edges).
  - When `Ensure Perimeter Contrast` is disabled, candidate hue is preserved directly without perimeter sampling or shifting.
- **Window Frame vs Content Separation**:
  - Added interior content tracking (`interiorCount`) in `processDominantAccent` so that window decoration borders/titlebars do not hijack the accent detection when the page itself is monochrome.

## [1.4.20] - 2026-10-21

### Fixed
- **Perimeter Contrast on White Windows**:
  - Fixed a critical regression in `enforcePerimeterContrast` where dark browser tab bars caused the minimum edge lightness to trigger the dark-window boost, erroneously forcing white outlines on white web pages.
  - White and light web pages now strictly maintain a crisp, high-contrast dark charcoal outline ($L \le 0.28$) or deep, rich accent colors, never blending into the page.
- **Vibrant Accent & Photo Colors (Google Images / Accents)**:
  - Eliminated washed-out, pale pastel mint tints ($L = 0.85$) on colorful pages like Google Images search results.
  - On light pages, chromatic outlines are guaranteed to be deep, rich, and punchy ($L \le 0.45, S \ge 0.80$).
  - Removed artificial 50% dampening in `applySaturationLeveling`, allowing chromatic accents to reach their full intended target saturation ($\ge 75\%$).
  - Corrected `processSmartAdaptive` background lightness reference so that white backgrounds pull chromatic outlines deeper into contrast rather than pushing them into white.
- **Sensitive Accent Detection**:
  - Tuned accent quorum thresholds (`minPixelQuorum` to ~0.4%, `minScoreQuorum` to 1.8) so that real UI accent elements—such as red underlines, colored badges, buttons, and links—are reliably detected and emphasized.

## [1.4.19] - 2026-10-21

### Fixed
- **Dominant Accent Quorum & Jitter Prevention**:
  - Implemented strict visual mass quorum requirements (`minPixelQuorum = std::max(4, validPixels / 35)` ~2.8% of window area, and `minScoreQuorum = std::max(2.5, validPixels * 0.025)`).
  - Prevents tiny isolated elements—such as 16x16 file icons in GitHub/file manager tree views, thin language/breakdown bars, and code syntax highlighting tokens (orange strings, blue keywords)—from hijacking the dominant accent and causing erratic color jumping when hovering or scrolling.
  - Increased previous hue inertia multiplier from `1.15` to `1.35` in `processDominantAccent` to eliminate rapid flip-flopping between close candidate hues.
- **Desktop Wallpaper Bleed on Rounded Window Corners**:
  - Inset ambilight sampling geometry by 8px inside client geometry (`w->clientGeometry().adjusted(inset, inset, -inset, -inset)`).
  - Eliminates desktop wallpaper (e.g. purple desktop background) leaking into the sampled pixel grid through the window's transparent rounded corner radiuses and drop shadows.
- **Perimeter Contrast Stability on Dark Windows**:
  - Revised `enforcePerimeterContrast` clash rules to strictly guarantee bright, high-contrast silver/white or vibrant neon outlines ($L \ge 0.80$) on all dark windows, preventing outlines from erroneously falling into dark charcoal gray ($L \le 0.25$) when header elements are present.
  - Restricted chromatic clash logic strictly to distinctly saturated edges ($S \ge 0.20$).

## [1.4.18] - 2026-10-21

### Added
- **Ensure Perimeter Contrast (Smart Edge Separation)**:
  - Added new option `AmbilightEnsureContrast` ("Ensure perimeter contrast (prevent blending with window edges)") with a dedicated checkbox in the Ambilight settings.
  - Dynamically samples the outer perimeter and title bar of the active window where the outline actually sits.
  - Resolves contrast loss on white windows with dark title bars: prevents the outline from sinking into dark charcoal against dark headers, boosting it into crisp bright silver/white ($L \ge 0.85$).
  - Resolves matching color blending on colored windows (e.g. green outline on a green window): automatically enforces $\Delta L \ge 0.40$ separation, turning the outline into a radiant electric neon pop ($L \ge 0.80, S \ge 0.80$) on dark/mid-toned windows, or a deep contrasting shade on light pastel windows.
  - Enabled by default across `Balanced (Default)`, `High Contrast`, `Vivid & Dynamic`, `Subtle & Calm`, and `Eco` profiles; explicitly disabled in `True Ambilight (Video / Cinema)` to preserve ambient scene lighting.

## [1.4.17] - 2026-10-21

### Fixed
- **Ambilight Slate Gray & Monochrome Handling**:
  - Eliminated faint blue/violet tint on gray and dark theme windows (KDE Breeze Dark, Nord, Catppuccin, One Dark, terminals, VS Code).
  - Raised achromatic deadband and chromatic binning thresholds to filter out slate-tinted canvas pixels (`s < 0.22`), strictly mapping gray/monochrome windows to pure neutral tones.
  - Resolved `Smart Adaptive` lightness ceiling issue where outlines on dark windows were trapped at dim mid-tones (`L <= 0.45`), allowing full contrast divergence.

### Changed
- **High Contrast Preset & Dynamic Range**:
  - Optimized the **High Contrast** Ambilight preset with expanded dynamic range (`minBrightness = 20`, `maxBrightness = 92`), delivering a brilliant bright silver/white outline (`L >= 0.88`) on dark windows and deep crisp charcoal outline (`L <= 0.25`) on light windows.
- **Smoother Corner Fading (Fade Corners)**:
  - Extended the fade transition distance from 45px to 120px for the "Fade out corners" feature, producing a significantly smoother, more gradual, and elegant falloff towards the window corners.

## [1.4.16] - 2026-10-21

### Added
- **Documentation**:
  - Comprehensive documentation added for **Ambilight (Adaptive Window Color)**, presets (`True Ambilight`, `Balanced`, `Vivid & Dynamic`, `Subtle & Calm`, `Eco`, `High Contrast`), and all fine-tuning parameters in `README.md`.
  - Documented the **Fade out corners (highlight edges only)** feature and default settings in `README.md`.

### Verified
- Verified edge illumination and smooth Hermite corner falloff behavior across varied window types and theme combinations.
- Confirmed full UI layout alignment and English localization consistency.

## [1.4.15] - 2026-10-21

### Changed

- **Default Setting for Fade Out Corners**:
  - Enabled "Fade out corners (highlight edges only)" (`FadeCorners=true`) by default for clean, modern aesthetics where only window edges illuminate with smooth corner falloff.

## [1.4.14] - 2026-10-21

### Fixed

- **KCM Configuration Layout Alignment**:
  - Fixed row index collision in `Appearance` tab where the version and update-checker row conflicted with the info banner at row 12, resolving layout displacement and preventing controls from jumping to the top of the window.
- **Language Consistency**:
  - Restored clean, standard English for the "Check for updates" button, update dialogs, and preset names.
  - Ensured the "Fade out corners (highlight edges only)" option and all associated tooltips display in English across all system locales.

## [1.4.13] - 2026-10-21

### Added

- **Fade Out Corners (Highlight Edges Only)**:
  - Added a new option `FadeCorners` ("Wyłącz narożniki (płynne zanikanie na rogach)") under the Appearance settings.
  - When enabled, the outline completely hides corner curves/vertices and smoothly fades out towards all 4 corners via a cubic Hermite gradient (`smoothstep`), highlighting exclusively the straight edges of the window.
  - Symmetrically fades out the soft neon glow and holographic glitch effects along the edges, preventing harsh or abrupt cutoffs.
  - When enabled, the Corner Radius spinbox is automatically grayed out in the settings UI.

## [1.4.12] - 2026-10-21

### Changed

- **Ambilight Sampling & Preset Cadence Optimization**:
  - Boosted preset sampling frequencies across all profiles to take full advantage of negligible GPU/CPU overhead:
    - **Balanced (Default)**: Boosted to 1.5 Hz (every 667 ms) with 0.5 s smooth transition.
    - **Vivid & Dynamic**: Boosted to 3.0 Hz (every 333 ms) with snappy 0.25 s transition.
    - **High Contrast**: Boosted to 2.0 Hz (every 500 ms) with 0.4 s transition.
    - **Subtle & Calm**: 1.0 Hz (every 1.0 s) with 1.0 s gentle crossfade.
    - **Eco (Battery Saver)**: Boosted to 0.5 Hz (every 2.0 s) with 1.0 s transition.
    - **True Ambilight (Video / Cinema)**: Boosted to 6.0 Hz (every 166 ms) with 0.15 s ultra-fast video follow.
- **Removed Unnecessary CPU Scare Notices**:
  - Replaced intimidating "increases CPU usage" warning in the footer with an informative, friendly tip explaining ideal frequency ranges for desktop vs. video tasks.
  - Renamed "Eco (Low CPU)" to "Eco (Battery Saver)" / "Oszczędzanie baterii (Eco)".
  - Updated tooltips and contextual help dialog in Polish and English to focus on responsiveness rather than artificial CPU concerns.

## [1.4.11] - 2026-10-21

### Added

- **"True Ambilight (Video / Cinema)" Preset**: Added a dedicated profile specifically tuned for dynamic video playback, live streams, and cinema in windowed mode:
  - **High-frequency sampling**: 5.0 Hz sampling rate (evaluates active window colors every 200 ms).
  - **Ultra-low latency transitions**: 0.2 s crossfade duration matches the 200 ms sampling interval for smooth, continuous color tracking with zero perceptible lag and no strobe-like eye strain.
  - **Balanced Average lighting**: Integrates scene lighting additively without erratic jumps from moving foreground objects or subtitles, and automatically down-weights black cinematic letterbox bars.
  - **Expanded dynamic range**: High saturation target (75%) and expanded brightness range (15% - 90%) for deep dark scenes and vivid daylight scenes.
  - Updated configuration UI and contextual help guides in Polish and English with full descriptions of the new profile.

## [1.4.10] - 2026-10-21

### Fixed

- **Outline bleed-through over notifications and Always-on-Top windows**: Fixed an issue where the active window outline was drawn on top of windows higher in the stacking order (such as notifications, OSD, "Keep Above / Always on top" windows, and overlapping dialogs):
  - In `renderOutline`, extended the occlusion clipping to all visible windows positioned above the outlined window in `effects->stackingOrder()`.
  - Notifications and OSDs now clip using their accurate blur/panel geometry, while standard windows clip using their frame geometry.
  - Connected `EffectsHandler::stackingOrderChanged` to immediately repaint and update outline occlusion when windows are reordered or promoted to KeepAbove.
  - Added dynamic repaint triggers in `slotWindowFrameGeometryChanged` and `slotWindowMinimizedChanged` when overlapping windows move or change state.

## [1.4.9] - 2026-10-21

### Fixed

- **Grayscale and slate-theme Ambilight color cast**: Eliminated unwanted grayish-violet/blue outline tint on dark gray and monochrome windows (e.g. KDE Breeze Dark `#232629`, Firefox `#2b2a33`, VS Code, dark terminals). Introduced an achromatic deadband in `applySaturationLeveling` ($s < 0.12$) that cleanly attenuates accidental cool slate temperature bias to pure neutral monochrome ($s = 0.0$).
- **Monochrome outline visibility and contrast**: Replaced same-luminance clamping with intelligent high-contrast luminance inversion (`getHighContrastMonochromeLuminance`):
  - Dark windows (dark mode, terminals, dark editors) now receive a bright, crisp silver outline ($L \approx 0.80 - 0.85$) instead of sinking invisibly into dark gray.
  - Light/white windows (documents, PDFs) receive a sleek, dark charcoal outline ($L \approx 0.25 - 0.30$).
  - Full contrast visibility is maintained across all presets.

## [1.4.8] - 2026-10-21

### Added

- **Check for Updates button ("Sprawdź aktualizacje")**: Added a dedicated update checker button in the settings panel next to the version indicator:
  - Queries GitHub API asynchronously for the latest release/pre-release.
  - Automatically compares semantic version numbers against the currently installed version.
  - Shows clear inline feedback and opens a dialog with the release notes, a one-click button to view on GitHub, and a "Copy Command" button for instant terminal updates (`curl ... | bash`).
  - Supports English and Polish localizations based on system language.

## [1.4.7] - 2026-10-21

### Added

- **Ambilight Presets ("Profile Ambilight")**: Added a presets dropdown selector with 5 preconfigured profiles and automatic "Custom" tracking:
  - **Balanced (Default)**: Optimized for everyday desktop use (Smart Adaptive, 1.0 Hz, 0.6s transition, 65% leveling, 50% fallback).
  - **Vivid & Dynamic**: Fast, aggressive accent tracking for videos and gaming (Dominant accent, 2.0 Hz, 0.3s transition, 80% leveling).
  - **Subtle & Calm**: Harmonious, gentle ambient glow (Balanced average, 75% opacity, 0.5 Hz, 1.2s transition, 45% leveling).
  - **Eco (Low CPU)**: Minimal CPU & battery consumption (Smart Adaptive, 0.2 Hz / once per 5s, 1.0s transition).
  - **High Contrast**: Maximum outline clarity and distinction (Smart Adaptive, 90% fallback distinction pop, 75% leveling).
  - **Custom**: Automatically selected when individual parameters are fine-tuned manually.
- **Help Button & Detailed Parameters Guide**: Added a contextual help button (`?`) in the Ambilight section that opens an interactive, formatted guide dialog with comprehensive explanations of each parameter, detection mode, and preset in Polish and English.
- **Tooltips on all Ambilight controls**: Added explanatory tooltips to all labels and input fields in the Ambilight settings.

## [1.4.6] - 2026-10-21

### Added

- **Saturation Leveling ("Wygładzenie saturacji")**: Replaced raw linear saturation multiplier with a target saturation leveling algorithm (`AmbilightSaturationLeveling`, 0–100%, default 65%). Instead of blowing out neon colors into harsh over-saturated hues or failing to lift pastels, leveling pulls low saturation gently upward while compressing extreme saturation downward towards the desired target level. Monochrome grays remain intact.
- **Fallback Distinction Strength ("Odróżnienie fallbacku")**: Added a dedicated slider (`AmbilightFallbackStrength`, 0–100%, default 50%) for the Smart Adaptive detection mode. Controls how boldly the outline modifies and separates itself from the window content when dominant colors align with the window average: from subtle blending (0%) to distinct contrast pop (100%). Dynamically enabled in the settings UI only when Smart Adaptive mode is selected.

## [1.4.5] - 2026-10-21

### Fixed

- **Multi-monitor Ambilight sampling freeze**: Fixed an issue in multi-monitor setups where `paintScreen` on an inactive screen or secondary output stole and cleared `m_ambilightSampleNeeded` before the primary screen containing the active window was painted, causing outline colors to hang or delay 5–10 seconds when changing page content.
- **Opportunistic zero-delay sampling**: Added rate-limited opportunistic sampling during active window repaints (respecting `AmbilightFrequency` interval), enabling immediate sampling and color transitions as soon as a webpage navigates or renders new content without waiting for an idle timer tick.
- **Transition repaint immediacy**: Invoked immediate outline repaint on starting Ambilight color transitions.

## [1.4.4] - 2026-10-21

### Added

- **Smart Adaptive (Hybrid) Ambilight mode**: Added a new intelligent hybrid detection mode set as default (`AmbilightMode = 0`):
  - Simultaneously tracks both the dominant accent and the balanced average of the window.
  - When the dominant accent matches or aligns with the overall window average (e.g. homogeneous themed apps or single-tone windows), it dynamically applies an extra vibrancy and contrast boost (up to +35% saturation pop) so the outline stands out boldly rather than blending into the window canvas.
  - When contrasting elements or diverse UI accents are present, it decisively maintains crisp dominant accent tracking.
- **Renamed detection modes**: Cleaned up UI terminology by removing the "Vivaldi" reference, resulting in three clean options: `Smart Adaptive (Hybrid)` (default), `Dominant accent`, and `Balanced average`.

## [1.4.3] - 2026-10-21

### Added

- **Ambilight mode toggle (Dominant accent vs Balanced average)**: Added configurable detection mode (`AmbilightMode`, default: `0` / Dominant):
  - **Dominant accent (Vivaldi)**: Discretizes window colors into 24 chromatic hue clusters and scores them by saturation ($s^{1.8}$) and midtone luminance. The most prominent vibrant accent wins decisively, producing punchy, aggressive accent tracking (e.g. YouTube red, Spotify green) that switches decisively when main content changes, with 15% inertia hysteresis to prevent jitter between close candidate colors.
  - **Balanced average**: Uses weighted additive RGB light mixing to smoothly blend all window elements into a calm, harmonious composite tone without aggressive jumps.
- **Config UI selector**: Added `Detection mode:` combo box in the Appearance tab Ambilight settings.

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
