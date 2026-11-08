# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses semantic versioning.

## [1.6.0-beta88-exp] - 2026-11-08

### Fixed
- **Tiling Slot Position Swapping Bug Across Workspace & Virtual Desktop Transitions**:
  - Fixed an issue where switching workspaces or virtual desktops inverted or swapped tiled window positions (e.g., column A and column B swapping to B and A) when returning:
    - **Per-Workspace & Per-Desktop Tiling State Preservation**: Added `WorkspaceTilingState` structure and `m_workspaceTilingStates` keyed by `currentTilingKey()` (`<wsId>:<desktopId>`). When switching workspaces or KDE virtual desktops, the outgoing slot assignments, window pointers, and pre-tiling geometries are fully preserved without deactivating or resetting window geometries.
    - **Deterministic 3-Pass Slot Continuity & Geometric Overlap Matching**:
      - Replaced naive linear assignment of `targetWindows` (which placed the active window into slot 0 regardless of spatial position, causing swaps whenever window B was focused) with a 3-pass stable algorithm:
        1. **Pass 1 (Slot Continuity)**: Windows already assigned to an existing slot retain their exact slot index.
        2. **Pass 2 (Geometric Overlap Matching)**: Unassigned windows match against unassigned slots by maximizing geometric intersection area (left window stays in left slot, right window stays in right slot).
        3. **Pass 3 (Spatial Fallback)**: Remaining unassigned windows are ordered top-to-bottom, left-to-right to match slots geometrically.
    - **Restoration without Geometry Deactivation**: `deactivateTiling(nullptr, true)` no longer moves windows or clears pre-tiling geometries when `preserveWorkspaceState == true`. Incoming workspaces seamlessly restore previous slot mappings via `restoreWorkspaceTiling()`.
    - **Virtual Desktop Switch Handling (`effects->desktopChanged`)**: Outgoing virtual desktop state is cached and incoming virtual desktop tiling state is cleanly restored without triggering layout swaps or resetting floating geometry.
    - **Deleted Window Cleanup**: `slotWindowDeleted()` cleans up closed window pointers across all cached workspace states.
  - Added unit test `testTilingSlotOrderPreservedAcrossWorkspaceSwitch` in `tests/tiling_test.cpp` verifying slot continuity across workspace switches and geometric overlap stability (all 32/32 tests passing).

## [1.6.0-beta87-exp] - 2026-11-07

### Added
- **Deterministic Fixed Letter Colors for Spatial Keyboard, Lasers & HUD Badges**:
  - Implemented `colorForLetter(QChar key)` in `keyboardoverlayhelper.h`, deterministically mapping each letter A-Z to a permanent, distinct, vibrant color from the 26-color palette.
  - Added configurable setting `KeyboardOverlayFixedLetterColors` (default: `true`) in configuration skeleton, KCM settings UI (`kcfg_KeyboardOverlayFixedLetterColors` in General Spatial Keyboard group), presets (`presethelper.h` and `presetactivationdialog.h`), and installation scripts.
  - When enabled, every letter key possesses a permanent distinct color:
    - **Spatial Keycap**: Keycap accent, outer glow, border, and the letter itself use its assigned color, establishing strong visual association and muscle memory habit. Unassigned keys show a subtle ghosted tint of their permanent color.
    - **Directional Laser & Targets**: Laser line, launch node, and window target letter badge adopt the letter's fixed color.
    - **HUD Badges**: The letter inside the HUD badge card (`renderOffscreenTargetPill`) and its inner accent border use the letter's fixed color.
    - **Window Marks Precedence**: Explicit user marks (`m_markedWindows`) take full precedence across all elements (spatial keycap, laser beam, target badge, and HUD card) over the fixed letter color.
  - Added comprehensive unit tests in `tests/keyboardoverlay_test.cpp` (`testFixedLetterColorsFeature`) and `tests/badge_test.cpp` (`testFixedLetterColorsInBadges`), verifying case insensitivity, uniqueness across all 26 keys, fallback handling, mark precedence override, and overlay/HUD rendering (all 32/32 tests passing).

## [1.6.0-beta86-exp] - 2026-11-06

### Added
- **Laser Targets Shortest Path Connection to HUD Badges**:
  - Replaced center-snapping (`hudCardRect.center()`) with continuous Euclidean shortest path calculation (`closestPointOnRoundedRectPerimeter`) for laser target arrows and HUD extension lines.
  - Directional laser arrows pointing to offscreen windows and dashed extension lines from on-screen window badges now connect directly to the nearest outer edge or rounded corner of the HUD badge pill rather than routing to the center, preserving badge visibility, shortening line length, and improving spatial clarity.
  - Added unit test `testLaserTargetsShortestPathToHudBadges` verifying orthogonal edge clamping, rounded corner projection, distance minimization, and overlay rendering (all 32/32 tests passing).
- **Window Frame Audio Effects (Pulse & Color Sequence Transitions)**:
  - Added native CPU-lightweight window outline effects when audio playback is detected (`hasPlayingAudio()`):
    - **Audio Frame Pulse (`AudioWindowFramePulseEnabled`)**: Smoothly modulates outline brightness and alpha with configurable breathing speed (`AudioWindowFramePulseSpeed`, 0.2x–3.0x) and intensity (`AudioWindowFramePulseIntensity`, 10%–100%).
    - **Audio Frame Color Sequence (`AudioWindowFrameColorSequenceEnabled`)**: Seamlessly cycles window outline color across customizable color stops (`AudioWindowFrameColorList`, e.g. `#f1c40f:2.0;#e67e22:1.5;#e74c3c:2.0`) with smooth interpolation duration (`AudioWindowFrameTransitionDuration`, 0.2s–5.0s).
    - Can be combined with or used instead of reactive GLSL shaders, obeying `AudioWindowFrameShaderMode` (`always`, `active`, `inactive`).
- **Dedicated Centralized "Audio & Sound" Settings Tab**:
  - Consolidated scattered audio and tactile sound settings into a single, clean dedicated tab (`tabAudio`):
    - **Group 1: Audio Window Frame Effects**: Mode (Always / Active / Inactive), GLSL shader toggle & selector, frame pulsing toggle & controls, and color sequence toggle & color stops.
    - **Group 2: Window Badge Audio Indicators**: Visual indicator styles (Speaker / Soundwave Pulse / Minimal Dot), pulsing animation toggle & speed/intensity sliders, and indicator scale.
    - **Group 3: HUD Audio Indicators**: Indicator style, HUD audio wave shader toggle, HUD card pulsing controls, and theme auto-color / custom color picker.
    - **Group 4: LeapK Interface Sound Effects**: Tactile sound effects toggle, sound profile selector (Crisp Click, Soft Pop, Sci-Fi Blip, Mechanical), volume slider, and instant "Play Test Sound" audition button.
- **Redesigned Nerd Stats CPU Overhead Presentation**:
  - Replaced ambiguous total system core utilization with isolated LeapK paint overhead in soothing green/cyan (`🧠 LeapK CPU: < 0.1% avg (idle) / 1.2% (optimal)`).

## [1.6.0-beta85-exp] - 2026-11-05

### Added
- **Laser Target Clean Mode & macOS-Style Middle-Elided Spatial Keycap Labels**:
  - **Screen Target Cleanliness**: Disabled floating window title labels next to laser targets on screen by default (`targetLabelVisible = false` in `OverlayDisplayConfig` and `themehelper.h`). The screen workspace remains completely unobstructed and clean, displaying only directional laser beams and circular window badges.
  - **Enlarged Keycaps with Dedicated Glassmorphic Label Pills**: Increased default keycap size from 54px to 64px (`keySize` bounded up to 96px in `computeOverlayGeometry` and settings UI). Structured keycap layouts into an upper bold letter zone and a dedicated lower contrast pill (`labelPill`) displaying the target window's title.
  - **macOS-Style Dynamic Middle Truncation (`elideMiddleTitle`)**: Window titles that fit within available keycap width are displayed 100% whole. When titles exceed keycap width, they are middle-elided using ellipsis (e.g. `"Terminal (bash) - neovim"` $\to$ `"Termin…neovim"`), dynamically preserving the application context and trailing document or task names.
  - Added comprehensive unit tests in `tests/keyboardoverlay_test.cpp` (`testKeycapMiddleElisionAndTargetLabelsDisabled`) verifying middle elision mathematics, tight width fallbacks, disabled target labels, and overlay rendering (100% passing tests, all 32/32 CTests).
- **Spatial Keyboard Toggle Shortcut (`Meta + L`) with Session Persistence**:
  - Added shortcut `Meta + L` in LeapK mode to toggle the spatial keyboard dock and laser targets on/off on the fly.
  - Setting is automatically persisted to user configuration (`primaryConfig.writeEntry("KeyboardOverlayEnabled", m_keyboardOverlayEnabled)` in group `Effect-leapk`) so user preferences are retained across KWin restarts and sessions.
  - Added dedicated cheatsheet entry in HUD Meta Overlay (`Meta + L: Spatial Keyboard [ON / OFF]`) with cyan glow and audible sound feedback (`playSpatialSound(SoundEvent::Toggle)`).
- **3-Mode Cycling for HUD Thumbnails (`Meta + Z`: Default -> Zoomed -> Off)**:
  - Expanded `Meta + Z` from a binary zoom toggle to a multi-state 3-mode cycle:
    - **Mode 0 (`DEFAULT`)**: Standard configured HUD thumbnail size (`cfg.hudShowThumbnail`).
    - **Mode 1 (`ZOOMED`)**: Enlarged 2x thumbnail previews (`effHudW = qMax(180, cfg.hudThumbnailWidth * 2)`).
    - **Mode 2 (`OFF`)**: Completely disables HUD thumbnails, providing a clean, compact HUD dock layout while eliminating thumbnail capture overhead (`m_hudThumbnailMode == 2`).
  - Automatically bypasses window thumbnail render passes and blits in `leapk_paint.cpp` when Mode 2 (`OFF`) is active.
  - Updated HUD Meta cheatsheet overlay pill for `Meta + Z` to dynamically reflect current mode (`DEFAULT`, `ZOOMED`, or `OFF`).
- **Cross-Workspace Taskbar / Dock Window Activation Interception (`SuperJumpCrossWsAction`)**:
  - When working inside a LeapK workspace (e.g. Workspace 1) and clicking an application icon on the KDE Plasma panel task bar / dock for a window that resides in a different workspace (e.g. Workspace 2) or Stash, LeapK intercepts the cross-workspace activation and presents the unified interactive modal prompt:
    - 🚀 **`[J]` Jump to Workspace**: Seamlessly switches to the target workspace and activates the window.
    - 📥 **`[M]` Move to Current Workspace**: Transfers the window into the current workspace and unminimizes it.
    - 📌 **`[P]` Pin to Both Workspaces**: Pins the window strictly to both the source and current workspace.
    - ❌ **`[Esc]` Cancel**: Re-minimizes the foreign window back into its workspace via workspace isolation, restores keyboard focus to the previous active window in the current workspace, and cleanly dismisses the HUD.
  - Fully honors `SuperJumpCrossWsAction` configuration:
    - Mode 0 (*Ask*, default): Interactive glassmorphic modal prompt.
    - Mode 1 (*Direct Jump*): Switches workspace view directly to the target window's workspace.
    - Mode 2 (*Direct Move*): Pulls the target window into the active workspace view immediately.
  - Windows already belonging to or pinned to the active workspace, as well as newly opened application windows, activate immediately without prompting.
  - Added unit test suite `testTaskbarCrossWorkspaceActivation` in `tests/superjump_test.cpp` verifying activation interception, cancellation isolation, focus restoration, and action execution across all modes (100% passing tests, all 32/32 CTests).
- **Stats for Nerds HUD Diagnostics Overlay (Moving Average Power & CPU)**:
  - Added dedicated glassmorphic "Stats for Nerds" real-time performance card in the HUD:
    - ⚡ **Leapk Power Draw**: Moving average power consumption in Watts computed from real-time GPU/CPU energy sensors (`/sys/class/hwmon/*/power1_average` or `power1_input`, or `/sys/class/power_supply/BAT*/power_now`) scaled by isolated LeapK render latency vs frame budget, displaying accurate LeapK average wattage (~0.05W–0.3W) and total package power.
    - 🧠 **LeapK CPU Overhead**: Isolated measurement of LeapK's own rendering pass (starting strictly after `effects->paintScreen()`, excluding KWin scene compositing and VSync/fence waiting). Formatted reassuringly in soothing green/cyan with contextual badges: `< 0.1% avg (idle)`, `1.2% avg (optimal)`, and `4.5% avg (low)`.
    - ⏱️ **Render Latency**: Rolling average LeapK render duration in ms (typically 0.2–0.5 ms) alongside screen refresh rate (Hz).
    - 💾 **Texture VRAM**: Live cached texture memory footprint (MB) across badges, icons, and window preview thumbnails.
    - 🎨 **GLSL Pipeline**: Active shader passes counter (glow, breathe, neon flicker, audio wave, custom shaders) and real-time hardware temperature (°C).
  - Added shortcut `Meta + D` in HUD to toggle the Stats for Nerds overlay on the fly with animated sound feedback.
  - Clicking on the card toggles it off.
  - Added config entry `NerdStatsEnabled` (boolean, default: `false`) in `leapkconfig.kcfg`, settings GUI `leapk_config.ui`, `leapk_config.cpp`, `install-leapk.sh`, and `uninstall-leapk.sh`.
  - Added cheatsheet entry in HUD Meta Overlay (`Meta + D: Nerd Stats`).
  - Added comprehensive unit test suite in `tests/nerdstats_test.cpp` verifying `RollingAverage` EMA mathematics, hardware sensor querying, and card rendering (100% passing tests, all 32/32 CTests).
- **Spatial Keyboard "Show only when tiling mode is active" Option (`KeyboardOverlayTilingOnly`)**:
  - Added new configuration toggle `KeyboardOverlayTilingOnly` (boolean, default: `false`).
  - Added dedicated checkbox in KCM settings GUI (`leapk_config.ui` under Spatial Keyboard & Laser Arrows group) and registered in KConfig skeleton (`leapk_config.cpp`), `leapkconfig.kcfg`, installer `install-leapk.sh`, and uninstaller `uninstall-leapk.sh`.
  - When enabled, the spatial keyboard overlay and laser arrows are restricted exclusively to when tiling layout mode is active (`m_tilingActive == true`).
  - When in standard floating window mode with `KeyboardOverlayTilingOnly` enabled, LeapK automatically falls back to standard HUD jump badges and group cards, avoiding any keyboard dock and laser arrow clutter.
  - Implemented unified state check `isKeyboardOverlayActive()` across jump key assignment, rendering, mouse hover/click hitboxes, and keyboard navigation.
  - Added unit test suite in `tests/keyboardoverlay_test.cpp` verifying overlay activation rules across floating and tiling modes, with 100% passing tests (all 31/31 CTests).

### Changed
- **Default Tiling Layout Shortcuts (`F` for Full Focus, `G` for 2x2 Quad Grid)**:
  - Updated default shortcut triggers in `defaultPresets()` (`tilinghelper.h`):
    - **`[F]` Full Focus**: Now mapped to `F` (mnemonic for *Focus* / *Full*).
    - **`[G]` 2x2 Quad Grid**: Now mapped to `G` (mnemonic for *Grid*).
  - Aligns the default tiling layout presets across the keyboard home row in natural order: `[A]` Two Columns, `[S]` Three Columns, `[D]` Master & Stack, `[F]` Full Focus, `[G]` 2x2 Quad Grid, and `[X]` Float.
  - Updated unit tests in `tests/tiling_test.cpp` (`testDefaultPresets` and `testToKdeJson`).

### Fixed
- **Flatpak, Container & Multi-Namespace Audio Tracking (Deezer, Spotify, VLC, Browsers)**:
  - Fixed issue where media players running inside Flatpak sandboxes (such as Deezer `dev.aunetx.deezer`) or containers were never detected as actively playing audio in the HUD, badges, or window frames.
  - Resolved Flatpak PID namespace isolation: Flatpak processes run with container-internal PIDs (e.g. 227) in PipeWire/PulseAudio and register MPRIS services via `xdg-dbus-proxy` (e.g. 401550), neither of which directly matched the host window PID (401611) in `/proc/<pid>/stat`.
  - Added multi-layered application and audio stream correlation in `audiotracker.h`:
    - **CGroup Scope Matching (`getPidCgroupScope`)**: Inspects `/proc/<pid>/cgroup` to match the exact systemd/cgroup scope shared across the window, D-Bus proxy, and Flatpak worker processes (e.g. `app-flatpak-dev.aunetx.deezer-*.scope`).
    - **Flatpak Portal & App Identity Tracking (`appNamesOrIdsMatch`)**: Parses `pipewire.access.portal.app_id` and `pipewire.access.portal.instance_id` in PipeWire dumps and pactl, mapping them to the window's `windowClass` (e.g. `dev.aunetx.deezer`).
    - **Direct DBus Property Retrieval (`GetAll`)**: Replaced raw `player.property("Metadata")` with DBus `org.freedesktop.DBus.Properties.GetAll` calls, preventing Qt D-Bus deserialization failures and warnings in system journal, cleanly extracting `xesam:title`, `DesktopEntry`, and `Identity`.
    - **Instant Activation Refresh**: Moved immediate audio refresh ahead of tracking checks in `updateAudioTrackingState` so external MPRIS playback events and Jump Mode entries trigger zero-latency audio detection.
  - Added unit tests in `tests/audiotracker_test.cpp` (`testAppNamesOrIdsMatching` and `testFlatpakAndCgroupMatching`) verifying Flatpak/CGroup matching, proxy PID handling, and multi-window disambiguation (100% passing tests, all 32/32 CTests).
- **Dynamic Real-Time Audio Reactivity & Instant Frame Reset**:
  - Fixed issue where window outlines and frames continued to signal audio playback long after audio was paused or stopped.
  - Implemented real-time PipeWire node state inspection (`parsePipeWireDumpStreams` via `pw-dump Node`): streams are only marked as playing when `state == "running"`, immediately dropping idle/suspended streams without waiting for PipeWire's 5-second `pulse.idle.timeout`.
  - Added authoritative MPRIS `PlaybackStatus` handling (`queryMprisPausedPids`): when a media player (Spotify, YouTube, VLC, browser) reports "Paused" or "Stopped", it is instantly excluded from active playback, ignoring stale uncorked sink inputs.
  - Connected D-Bus signals (`PropertiesChanged` on `org.mpris.MediaPlayer2.Player` and `NameOwnerChanged`) for immediate zero-latency (< 1 ms) reaction on pause, play, or player exit.
  - Replaced the sluggish, fixed 1500 ms polling timer with adaptive dynamic polling (`updateAudioTrackingState`):
    - Fast **150 ms** dynamic refresh interval while audio is actively playing or audio frame effects are engaged to detect stops almost instantaneously.
    - Relaxed **800 ms** interval during silent periods, keeping CPU usage near zero.
    - Active outside LeapK Jump Mode whenever audio-reactive frame rules (`AudioShaderRuleEnabled`) or badges are active.
  - Filters out LeapK's own process ID (`selfPid`) so LeapK feedback sound effects never trigger outline audio states on windows.
  - Added unit test `testPipeWireDumpParsing` in `tests/audiotracker_test.cpp` verifying real-time running/idle node state filtering and PID isolation (100% passing tests, all 32/32 CTests).
- **HUD Badge Occlusion Protection from Laser Targets & Arrow Badges**:
  - Prevented spatial keyboard laser target arrows, circular target badges, and window title labels from covering or obscuring HUD cards and window badges in the HUD dock.
  - When targeting offscreen windows whose representations reside in the HUD dock (`isTargetInHud == true`), the laser arrow terminates neatly at the outer perimeter edge of the HUD card (`item.hudCardRect.top() - 3.0` or `bottom() + 3.0`) with a directional arrowhead pointing to the card, completely omitting circular target badges, borders, and letters over the HUD card to ensure the window's badge and preview remain 100% visible and unoccluded.
  - For on-screen windows, connector lines terminate at the perimeter border rather than the card center, and `computeTargetBadgePlacements` and `computeTargetLabelPlacements` treat `layout.hudBounds` and all `hudCardRect`s as protected obstacles, clamping coordinates and shifting labels away from the HUD dock.
  - Added unit test `testLaserTargetsAvoidHudBadges` in `tests/keyboardoverlay_test.cpp` verifying collision avoidance and clean HUD card isolation.
- **Interactive Cross-Workspace Selection in Window Search Mode**:
  - Unified the window selection flow between Search Mode and Super Jump Mode (`Meta + A`): selecting a window that belongs to a different workspace (or Stash / other desktop) in Search Mode now invokes the same interactive cross-workspace modal prompt (`[J]` Jump to workspace, `[M]` Move to current workspace, `[P]` Pin to both workspaces, `[Esc]` Cancel / Back to Search).
  - Wired cross-workspace checking into Search Mode key handlers (pressing `Return`/`Enter` or `Alt + JumpKey`) and mouse click handlers on HUD dock pills.
  - Refined `isCrossWorkspaceTarget` to identify cross-workspace windows in both Super Jump and Search Mode while correctly respecting windows pinned to the active workspace.
  - Updated hint bar text in `badgehelper.h` to display `Cancel / Back` to accurately reflect both Super Jump and Search Mode contexts.
  - Added unit test `testSearchModeCrossWorkspaceSelection` in `tests/superjump_test.cpp` verifying cross-workspace detection and interactive modal triggering in Search Mode (100% passing tests across all 32 CTests).
- **Multi-Instance Window Workspace Assignment & Isolation (e.g. Multiple Dolphin / Konsole Windows)**:
  - Fixed bug where opening a new instance of an already open application (such as a second Dolphin or Konsole window) in another workspace caused LeapK to ignore the new window in that workspace, omit HUD cards/badges, and improperly minimize or isolate it.
  - Introduced runtime instance mapping `m_windowWorkspace: QMap<EffectWindow *, int>` to track each window object independently, ensuring separate window instances of the same application class can belong to different LeapK workspaces simultaneously.
  - Updated `slotWindowAdded` to immediately register new windows into the active workspace view (`m_windowWorkspace[w] = m_activeHudView`), ensuring subsequent queries and delayed window title/class events properly preserve workspace assignment.
  - Enhanced `addWindowToWorkspace` to detect when other instances of the same application class exist in other workspaces (`otherWindowInOtherWs`), preserving existing workspace rules instead of wiping them across all workspaces, while ensuring the target workspace also has a valid rule.
  - Updated `findWorkspaceForWindow` to check the runtime instance map first, falling back to static workspace rules and caching the result.
  - Replaced static `matchesWorkspace` rule checks with `findWorkspaceForWindow(w)` in `applyWorkspaceViewIsolation`, `assignJumpLetters`, HUD header tabs count, tiling slot assignment, and workspace deletion logic.
  - Added unit test suite `testMultiInstanceWindowWorkspaceAssignment` in `tests/workspace_test.cpp` verifying multi-instance window coexistence, dynamic workspace resolution, window isolation, HUD card counts, and workspace deletion index shifting (100% passing tests across all 32 CTests).
- **Cross-Workspace Selection: "Pin / Both Workspaces" Option Pinning to All Workspaces**:
  - Fixed issue where selecting `[P]` / `[3]` (*Pin / Both Workspaces*) from the cross-workspace interactive modal dialog invoked `pinWindow(target)` which assigned `{0}` (all workspaces), causing the window to be pinned to every single workspace.
  - Implemented `pinWindowToWorkspaces(EffectWindow *w, const QSet<int> &wsIndices)` to pin the window strictly to the target window's original workspace and the current active workspace (`{targetWs, currentWs}`).
  - Added intelligent fallback resolution for `m_crossWsTargetWorkspace` and `m_crossWsCurrentWorkspace` when detecting cross-workspace windows.
  - Updated dialog prompt text in `badgehelper.h` to explicitly display *"Pin window to both [Source] and [Current]"*.
  - Added unit tests in `tests/superjump_test.cpp` and `tests/workspace_test.cpp` verifying dual-workspace pinning isolation without setting global pin (`0`).
- **Workspace-Isolated Pinning Mechanism Rework**:
  - Pinned windows now belong purely to LeapK workspaces (all workspaces or user-chosen specific workspaces) without altering or integrating with KDE virtual desktops.
  - Completely eliminated `effects->windowToDesktops(w, {})` (empty list = all KDE virtual desktops) in `pinWindow` and desktop forcing in `unpinWindow`: windows stay on their original virtual desktop within the selected workspaces.
  - Introduced `m_windowPinnedWorkspaces: QMap<EffectWindow *, QSet<int>>` and `isWindowPinned(w, wsIndex)` supporting per-workspace pin assignments as well as global pinning (`wsIndex = 0` / all workspaces).
  - Added interactive Pin Mode modal dialog (`renderHudPinWorkspaceDialog`):
    - When selecting a window in Pin Mode (`Meta + P` followed by window jump key or clicking its HUD dock card / keycap), LeapK displays an interactive glassmorphic modal listing:
      - `[A]` / `[0]`: **All Workspaces** (`✓ ALL`)
      - `[1]` .. `[N]`: Specific workspaces (`✓ PINNED` indicator and individual toggle support)
      - `[U]`: **Unpin Window** (if window is currently pinned)
      - `[Esc]` / `[Backspace]`: **Cancel** & return to Pin Mode
    - Full keyboard and mouse support with hover highlights and responsive hitboxes.
    - Added comprehensive unit tests in `tests/workspace_test.cpp` and `tests/superjump_test.cpp` verifying modal dimensions, row counts, hit rects, desktop isolation, and pin toggle logic.
- **HUD Thumbnails Quick Zoom Toggle (`Meta + Z` / `Meta + Space`, `Meta + Z`)**:
  - Instant shortcut to enlarge window thumbnail previews in the HUD dock, regardless of whether thumbnails are small or disabled in settings (`BadgeHudShowThumbnail = false`).
  - Shortcut: `Meta + Z` while in HUD (or outside HUD to trigger HUD with zoomed thumbnails).
  - Enlarges thumbnail dimensions to `qMax(180, cfg.hudThumbnailWidth * 2)` x `qMax(110, cfg.hudThumbnailHeight * 2)` with proportional pill sizing.
  - Added `Meta+Z: Zoom Thumbnails` with active indicator to the holding-Meta shortcuts cheatsheet overlay (`renderHudMetaOverlayImage`).
  - Added unit test validation in `tests/superjump_test.cpp` and verified with 100% passing test suites (all 31/31 CTests).

## [1.6.0-beta84-exp] - 2026-11-04

### Added
- **Experimental Spatial Keyboard Overlay with Window Directional Arrows (`Meta + Space`)**:
  - Displays a glassmorphic QWERTY keyboard layout dock (`QWERTYUIOP`, `ASDFGHJKL`, `ZXCVBNM`) on the screen upon pressing `Meta + Space`.
  - Keys without any assigned window are rendered strongly semi-transparent / ghosted (`QColor(28, 32, 42, 55)` with faint letters).
  - Keys assigned to active windows are brightly illuminated with glowing borders and distinct neon colors from a 26-color high-contrast palette.
  - From each active key, a neon laser arrow line with drop-shadow outline and swept-back arrowhead originates and points directly to the center of its assigned window.
  - Every arrow is guaranteed a distinct, unique color (`każda strzałka musi mieć inny kolor`).
  - At each window center, a circular target ring with the letter and an app class caption pill is rendered.
  - Spatial key matching algorithm (`assignSpatialKeys`): automatically assigns windows to keys corresponding to their physical screen position (left windows -> left keys, right windows -> right keys, top windows -> top row) to prevent arrow lines from crossing.
  - Interactive mouse and keyboard support: pressing the key or clicking the key on the keyboard overlay immediately focuses, raises, and jumps to that window.
  - Added dedicated unit tests in `tests/keyboardoverlay_test.cpp` verifying color uniqueness, layout computation, spatial assignment, and image rendering. All 31/31 CTests pass (100%).
- **Interactive Cross-Workspace Selection Action Menu (`SuperJumpCrossWsAction`)**:
  - When in Super Jump mode (`Meta + Space` followed by `Meta + A`) or selecting a window across workspace boundaries, LeapK now displays a fast, interactive action dialog instead of abruptly jumping or moving without confirmation.
  - Three single-key action choices with high-visibility styling, clear descriptions, and responsive keycaps:
    - `[Enter]` or `[J]` or `[1]`: **Jump to Workspace** (Default, switches to target workspace/desktop and focuses the window).
    - `[M]` or `[2]`: **Move to Current Workspace** (Pulls the target window directly into the current workspace and desktop).
    - `[P]` or `[3]`: **Pin / Both Workspaces** (Pins the window across all workspaces and virtual desktops).
    - `[Esc]`, `[Backspace]`, or `[Space]`: **Return back to Super Jump** (Dismisses the menu back to window selection without losing context).
  - Unassigned keypresses trigger visual/auditory invalid-key feedback (`triggerInvalidKeyFeedback`).
  - Full mouse support: interactive hover states (`PointingHandCursor`) and click execution on any action row, or click outside/right-click to cancel.
  - New configurable setting under KCM / Settings / Presets (`SuperJumpCrossWsAction`):
    - `0`: `Interactive Action Menu (Ask)` (Default / Recommended)
    - `1`: `Direct Jump to Workspace (Classic)`
    - `2`: `Direct Move to Current Workspace`
  - Added unit test coverage `testHudCrossWorkspaceActionDialogRendering`, `testCrossWorkspaceActionModes`, and `testCrossWorkspaceActionDialogShortcuts` in `tests/superjump_test.cpp`. All 31 CTests pass (100%).

## [1.6.0-beta83] - 2026-11-04

### Fixed
- **Tile Edit Mode Resize Overhaul with Shift Edge Selection**:
  - `h` (Left) and `l` (Right) widen or narrow the active tile slot from one edge, while `Shift + h` and `Shift + l` widen or narrow from the opposite edge.
  - Windows touching the right screen boundary default to operating on their left divider (`h` widens to the left, `l` narrows to the right).
  - Windows touching the left screen boundary default to operating on their right divider (`l` widens to the right, `h` narrows to the left).
  - Pressing `Shift` inverts which edge/divider is operated on: middle windows default to their right divider, while `Shift` targets the left divider. If a window touches a boundary without a divider on that side, it safely falls back to the existing divider.
  - Symmetric logic for vertical height resizing: `j` (Down) and `k` (Up) default to the bottom divider, while `Shift + j` and `Shift + k` operate on the top divider. Windows touching the screen top operate on their bottom divider, and windows touching the screen bottom operate on their top divider.
  - Updated tile edit HUD hint bar to clearly indicate: `[H/L: Width  J/K: Height (Shift: other edge)]`.
- **Universal Window Rule for Minimum Size (`types=-1`, `ignoregeometry=true`)**:
  - Fixed issue where dialog windows and setup utilities (e.g. CachyOS Hello `cachyos-hello`) refused to shrink below default minimum dimensions because `types=1` in `kwinrulesrc` only matched `NET::NormalMask` windows, ignoring `NET::DialogMask` (32), `NET::UtilityMask` (256), and `NET::SplashMask` (512).
  - Changed `types` to `-1` (`NET::AllTypesMask`) in `tilinghelper.h`, `install-leapk.sh`, and `~/.config/kwinrulesrc` so rules match all application window types.
  - Added `ignoregeometry=true` (`ignoregeometryrule=2`) alongside `strictgeometry=false` (`strictgeometryrule=2`) and `minsize=1,1` (`minsizerule=2`) to force KWin to bypass client-specified minimum and strict geometry constraints.
- **Aggressive Window Badge Placement (Avoid HUD as Last Resort)**:
  - Partially covered windows during Super+J navigation no longer prematurely fail discrete anchor positions and get banished to the HUD dock.
  - Implemented multi-phase geometric `QRegion unoccluded` subtraction in `badgehelper.h` and `leapk_hud.cpp`: subtracts higher stacking-order occluding windows and active HUD collision rectangles from the candidate window geometry.
  - Phase A: Finds the largest unoccluded rectangle accommodating badge dimensions and centers the badge within it.
  - Phase B: Evaluates 5 anchor points in all visible unoccluded rectangles having at least 80% badge dimensions.
  - Phase C: Tests the unoccluded region's bounding rectangle center.
  - Phase D: Fallback for windows with at least 1.2x badge area visible. Banishing to the HUD dock is strictly preserved as an absolute last resort when the window is completely covered.
  - Added unit test coverage in `tests/badge_test.cpp` (Case 6) and `tests/tiling_test.cpp` (`testShiftAndBoundaryEdgeResize`). All 30/30 CTests pass (100%).

## [1.6.0-beta82] - 2026-11-04

### Fixed
- **Badge Position "Bottom" Fallback Fix**:
  - Excluded dock windows (`candidate->isDock()`), notifications, popup windows, tooltips, lock screen, and input method windows from `isBadgeOccluded`. KDE Plasma's panel dock at the bottom of the screen no longer falsely occludes window badges and forces them to center.
  - Restricted `m_hudHeaderScreenRect` and `m_hudHintBarScreenRect` collision checks in `isBadgeOccluded` to only occur when HUD is actively rendered or in HUD preview.
  - Implemented smart alternative position ordering in `findBestBadgePositionForWindow` and `OutlineBadge::findBestBadgePosition`: windows configured with Bottom positions (`Bottom`, `Bottom-Left`, `Bottom-Right`) now prefer neighboring bottom positions first rather than immediately jumping to `Center`.
- **Accurate Window Swapping in Tile Edit Mode**:
  - Implemented `refreshTilingSlotWindows()` and `findWindowInSlot()` to dynamically associate slots with visible application windows based on maximum geometric intersection area.
  - Resolved window overlapping/covering bug when pressing slot letters in edit mode: both windows (`w1` and `w2`) are correctly retrieved, swapped, positioned in their new slots, and raised in the window manager.
- **Asymmetric Middle Window Resizing in Tile Edit Mode**:
  - Updated `resizeSlotGeometries` in `tilinghelper.h`: middle slots with both left and right dividers now move a single divider in the direction requested (Right moves the right divider right, Left moves the right divider left with fallback to the left divider) rather than dividing delta symmetrically. Vertical middle slots similarly move a single divider down or up.

### Added
- **Dynamic Split, Merge, Float, and Delete in Tile Edit Mode**:
  - `Ctrl + V`: Split active tile slot vertically into top and bottom halves (`splitActiveTileSlot(Qt::Vertical)`).
  - `Ctrl + H`: Split active tile slot horizontally into left and right halves (`splitActiveTileSlot(Qt::Horizontal)`).
  - `Ctrl + M`: Interactive merge mode (`m_tilingMergeModeActive`) allowing the user to press the letter of an adjacent tile slot to merge them into a single bounding rect (`mergeTileSlots()`), with `Esc` to cancel.
  - `Ctrl + F`: Toggle floating window for the active window in tile edit mode (`slotToggleActiveWindowFloating()`).
  - `Ctrl + D`: Delete active tile slot (`deleteTileSlot()`), cleanly floating any occupant and expanding the best adjacent neighbor to absorb the vacated space.
  - Updated tile edit overlay HUD hint bar to display `[Ctrl+V/H: Split V/H] • [Ctrl+M: Merge  Ctrl+F: Float  Ctrl+D: Delete]`.
  - Added unit test `testDynamicSplitMergeDelete` and updated `testThreeColumnsMiddleSlotResize` and `testHudObstacleBadgeAvoidance`. All 30/30 CTests pass (100%).

## [1.6.0-beta81] - 2026-11-04

### Fixed
- **Comprehensive Uninstaller Cleanup (`uninstall-leapk.sh` / `uninstall.sh`)**:
  - Automatically and cleanly purges the universal tiling window rule (`a9e701bc-6258-4903-a185-3b95a8bfb621`) from `~/.config/kwinrulesrc`, safely removing the rule section, updating `rules=` in `[General]`, and adjusting `count=`.
  - Completely strips all LeapK configuration sections (`[Effect-leapk]`, `[Effect-active-window-outline]`) from `~/.config/kwinrc` to eliminate orphan section headers and dynamic backup entries (`CustomTilingBackup_*`).
  - Added cleanup for all registered shortcuts in `~/.config/kglobalshortcutsrc`, including `ToggleActiveWindowFloating` and `ToggleAiPrompt`, purging residual shortcut groups and reloading `KGlobalAccel` via D-Bus.
  - Added real-time notification to running KWin instances via D-Bus (`org.kde.KWin /KWin reconfigure`) during uninstallation to immediately unload window rules and refresh system state.
  - Added test coverage in `tests/configsync_test.py` verifying uninstaller coverage for window rules and shortcuts. All 30/30 CTests pass (100%).

## [1.6.0-beta80] - 2026-11-03

### Fixed
- **Enforce No Minimum Window Size Limits for Tiling (`strictgeometry=false`, `minsize=1,1`)**:
  - Eliminated window protruding issues (e.g. Firefox protruding outside tile boundaries) caused by applications enforcing internal minimum window size constraints.
  - Enhanced the universal KWin tiling rule (`a9e701bc-6258-4903-a185-3b95a8bfb621`) with `strictgeometry=false` (`strictgeometryrule=2`) and `minsize=1,1` (`minsizerule=2`).
  - Added immediate re-evaluation of window rules for all active windows (`win->evaluateWindowRules()`) and KWin D-Bus reload trigger.
  - Added automated rule provisioning directly to `install-leapk.sh` for clean KDE installations.
  - Added `TilingRemoveMinSizeLimit` setting (enabled by default: `true`). All 30/30 CTests pass (100%).

## [1.6.0-beta79] - 2026-11-03

### Fixed
- **Forward Compatibility with KDE Plasma 6.9 / KWin Git Master**:
  - Added `LeapKCompat::clientArea` using C++20 `if constexpr (requires { ... })` to seamlessly adapt to KWin 6.9's updated `EffectsHandler::clientArea` signature (where the `VirtualDesktop*` parameter was dropped in favor of `clientArea(option, screen)`).
  - Supported modern `KWin::Rect` and `KWin::RectF` geometric primitives, ensuring safe conversion to `QRectF` across `clientArea` and `virtualScreenGeometry`.
  - Added `qt6-svg-dev` dependency into `install-leapk.sh` for Ubuntu, Debian, and KDE Neon distros. All 30/30 CTests pass (100%).

## [1.6.0-beta78] - 2026-11-03

### Fixed
- **Native Recursive KDE Split Trees Instead of Floating Tiles (`buildKdeTileTree`)**:
  - Fixed tile editor behavior where adjacent tiles had independent margins and could overlap rather than sharing a single synchronized resize divider.
  - Eliminated the fallback to `"layoutDirection": "floating"` in `toKdeJson()`.
  - Implemented recursive binary/n-ary space partitioning (`buildKdeTileTree`) that converts arbitrary tile slot geometries into native KDE Plasma 6 hierarchical trees (`"horizontal"` and `"vertical"` containers with leaf tiles summing to 1.0).
  - Added proactive sanitization in `activateTilingPreset`, `syncActiveTilingSlotsToKdeTiles`, and `onKdeTilesEditorClosed` to purge any stale `"floating"` layout entries from `~/.config/kwinrc` and in-memory caches.
  - Added unit test coverage in `tests/tiling_test.cpp` verifying recursive split trees for 3-column, 2x2 grid, and master-stack layouts. All 30/30 CTests pass (100%).

## [1.6.0-beta77] - 2026-11-03

### Added
- **Full LeapK Theme Engine Integration in Tile Edit Mode & Toasts**:
  - Connected `LeapKTheme::Theme` (`activeTheme`) into `renderFloatingPillImage`, `renderFloatingPill`, `renderTilingEditOverlay`, and `renderTilingToast`.
  - Tile swap target badges ('A', 'B', ...) and active slot badges ('EDIT') now inherit theme accent colors (`theme.primaryAccentColor()`, `theme.secondaryAccentColor()`), contrast-aware text styling (`contrastingTextColor`), and custom badge shapes (`pill`, `rounded_rect`, `chamfered`, `sharp`, `circle`) defined in `theme.windowBadge.shape`.
  - The bottom hint bar and toast notifications now adopt `theme.surfaceBackgroundColor()`, `theme.hudContainer.borderColor`, `theme.hudContainer.borderWidth`, `theme.hudContainer.shadowEnabled`, keycap colors from `theme.hintsBar.keyColor`, and container shapes from `theme.hudContainer.shape`.
  - Added unit test coverage in `tests/badge_test.cpp` verifying themed floating pill rendering with chamfered and sharp shapes and custom accent tokens. All 30/30 CTests pass (100%).

## [1.6.0-beta76] - 2026-11-03

### Fixed
- **Uniform High-DPI Floating Pill Badges in Tile Edit Mode (`renderFloatingPill`)**:
  - Replaced legacy `renderBadge` with dedicated `renderFloatingPill` for tile swap letter targets ('A', 'B', etc.) in `renderTilingEditOverlay`.
  - Eliminated distortion and stretching caused by `renderBadge` incorporating target window icons/thumbnails and falling back to cached window badges.
  - Added square/circular aspect ratio auto-expansion for 1–2 character floating pills (`imgW = qMax(imgW, lineH + 2 * padY)`).
  - Enhanced font rendering with `QFont::PreferAntialias`, `QPainter::SmoothPixmapTransform`, and increased font pixel scaling (`1.25 * scale`) for ultra-sharp typography at high DPI.
  - Refined horizontal and vertical baseline alignment to center single-letter swap badges perfectly within pill boundaries.

### Added
- **Unit Test Coverage for Single-Letter Swap Pills**:
  - Added test case assertions in `tests/badge_test.cpp` verifying single-letter floating pills render with circular/pill geometry and sharp anti-aliasing. All 30/30 CTests pass (100%).

## [1.6.0-beta75] - 2026-11-03

### Fixed
- **Crisp 1:1 Pixel Floating Pill Renderer (`renderFloatingPillImage` & `renderFloatingPill`)**:
  - Eliminated blurriness, resolution distortion, and horizontal text truncation in Tile Edit mode (`Meta + Enter`) and HUD toasts.
  - Removed inappropriate `renderBadge` usage for hint bars, active slot indicators, and toasts: `renderBadge`'s hardcoded 160px text elision cap caused long shortcut instructions and toasts to be truncated and stretched across 760–780px wide OpenGL quads.
  - Introduced `OutlineBadge::renderFloatingPillImage` and `ActiveWindowOutlineEffect::renderFloatingPill`, which dynamically measure text using `QFontMetrics`, parse bracketed shortcut keycaps (e.g. `[Ctrl+S]`, `[K]`, `[Enter]`) into styled bold cyan tags, and render crisp rounded pills at exact 1:1 pixel coordinates with zero stretching.
  - Fixed tile swap letter badge aspect ratio (1.35) in `renderTilingEditOverlay` so swap letters are no longer squished horizontally onto square quads.
  - Updated `ActiveWindowOutlineEffect::renderBadge` to always map vertices using the native texture dimensions centered on the target rect, guaranteeing aspect ratio preservation and 1:1 pixel accuracy across all HUD badges.
- **Direct Live `RootTile` Layout Synchronization on KDE Tiles Editor Exit**:
  - Fixed bug where exiting the KDE Tile Editor failed to apply layout changes because `refreshTilingPresetsWithKdeCustom` fell back to stale `kwinrc` or old backup entries instead of reading the editor's live in-memory tiles.
  - `onKdeTilesEditorClosed` now flushes `tileMgr->saveSettings()`, extracts leaf tile geometries directly from live `rootTile->descendants()`, geometrically sorts them in reading order, updates `[K] KDE Custom`, and synchronizes the fresh layout into `CustomTilingBackup`.
  - Added matching check in `activateTilingPreset`: if `rootTile` already contains the user's fresh layout, in-memory tiles are preserved without wiping or re-reading from disk.
  - Prioritized existing tiled windows in `m_activeTilingSlots` during window placement so windows immediately snap and rearrange into the new KDE layout upon exiting the editor.

### Added
- **Unit Test Coverage for Floating Pill & Hint Bar Typography**:
  - Added unit test assertions in `tests/badge_test.cpp` verifying `renderFloatingPillImage` produces sharp, non-elided full-length images, handles styled keycaps, and scales appropriately for HiDPI displays. All 30/30 CTests pass (100%).

## [1.6.0-beta74] - 2026-11-03

### Added
- **Dedicated `[K] KDE Custom` Bridge & Reactive Tile Editor Integration**:
  - Reassigned the KDE Custom live bridge shortcut from `C` to **`K`** (`[K] KDE Custom`). Key `K` is permanently protected against deletion and reserved in `leapk-custom-layout`.
  - `[K] KDE Custom` is guaranteed to be permanently present in the Tiling Presets list (`Meta + T`), serving as a live bidirectional bridge to KDE's native `TileManager::rootTile()`.
- **Automatic Re-tiling & Non-intrusive Toast Hint on KDE Editor Exit**:
  - Added background watcher for KWin's built-in `tileseditor` effect.
  - When the KDE Tile Editor opens (either via `Ctrl + E` in `Meta + T` or via KDE's native shortcut), LeapK cleanly yields focus, releases keyboard grab, and hides all overlays/HUDs to avoid UI conflict.
  - When the KDE Tile Editor is closed (`Esc` or apply), LeapK instantly:
    - Reads the freshly configured grid from KDE's `RootTile`.
    - Updates `[K] KDE Custom` with the new slot geometries.
    - Automatically retiles and arranges open windows on the current desktop into the new layout.
    - Displays a non-intrusive, sleek HUD toast notification:
      `▤ Layout updated: [K] KDE Custom • Press [Ctrl+S] in edit mode to save as preset`.
- **Save Flow & Overwrite Existing Preset Support**:
  - In `Ctrl + S` (`SaveLayoutDialog`), if the active preset is a user layout (e.g. `M`), the dialog pre-fills key `M` and name so pressing Enter immediately updates/overwrites that preset with the new structure.
  - If the active preset is `[K] KDE Custom`, the dialog pre-fills the first free letter (e.g. `H`), preserving `[K]` as the bridge.
- **Eliminated KWin TileManager Ghost Tile Accumulation**:
  - Cleared existing child tiles via `rootTile->destroyChild()` before calling `tileMgr->readSettings(rootTile)` across all resize and layout sync operations, eliminating conflicting ghost and nested tiles.
- **Removed `Ctrl + E` from Tile Edit Mode (`Meta + Enter`)**:
  - Streamlined `Meta + Enter` strictly to in-place live window resizing (HJKL/arrows/+/-), window swapping (letters), and saving (`Ctrl + S`), eliminating the conflicting transition to the KDE editor.

## [1.6.0-beta73] - 2026-11-03

### Fixed
- **Symmetric Keyboard Resizing for Middle Tiles (`tilinghelper.h`)**:
  - Fixed critical bug where resizing internal tiles (such as the middle column in a 3-column split) caused both adjacent columns 1 and 3 to suddenly crush and collapse.
  - Implemented symmetric divider adjustment for middle slots with dividers on both sides (`hasLeftDivider && hasRightDivider`):
    - Expanding (`L`, `Right`, `+`, `=`) widens the active middle tile by `delta` by shifting the left divider left (`-delta/2`) and the right divider right (`+delta/2`), evenly distributing space from both neighbors.
    - Shrinking (`H`, `Left`, `-`, `_`) narrows the active middle tile by `delta` by shifting the left divider right (`+delta/2`) and the right divider left (`-delta/2`), evenly distributing space back to both neighbors.
    - Resizing is fully reversible: expanding and then shrinking returns tiles precisely to their original geometries without drift or boundary overflow.
    - Middle rows with top and bottom dividers (`hasTopDivider && hasBottomDivider`) now expand and shrink symmetrically vertically.
- **Context-Aware `J` Key & Intuitive Resize Controls (`leapk_input.cpp`, `leapk_tiling.cpp`)**:
  - Context-aware `J` handling: if the active slot has no vertical neighbors (e.g. middle column in 3 columns, or Master in Master & Stack), `J` acts as `Left` (Narrower/Shrink) to match IJKL user navigation habits; if vertical neighbors exist, `J` expands height (`Down`).
  - Added dedicated size shortcuts: `+` / `=` / `]` for Wider, `-` / `_` / `[` for Narrower.
  - Added `Shift` edge targeting modifier: holding `Shift` with navigation keys targets specific dividers (`Shift + Left`, `Shift + Right`, etc.).
  - Updated Tile Edit HUD hint bar: `[H/←: Narrower  L/→: Wider  K/↑: Shorter  J/↓: Taller]  •  [+/-: Size]  •  [Ctrl+S: Save layout]  •  [Letters: Swap]  •  [Enter: Done]`.
- **Automated Regression Suite (`tests/tiling_test.cpp`)**:
  - Added `testThreeColumnsMiddleSlotResize` unit test verifying 3-column middle tile keyboard resizing, symmetrical expansion and shrinkage, exact reversibility, minSize boundary clamping, and targeted edge modifiers (`TileEdge::Right` and `TileEdge::Left`).

## [1.6.0-beta72] - 2026-11-03

### Fixed
- **Multi-Column Mouse Tiling Resize & Boundary Divider Targeting**:
  - Fixed regression in interactive tiling mouse resize where resizing the middle slot in multi-column layouts (such as 3 columns) caused unrelated adjacent slots (e.g. Firefox on the left) to jump and collapse.
  - Introduced `TileEdge` targeting in `calculateDividerMove` and `resizeSlotGeometries` (`tilinghelper.h`):
    - Differentiates between dragging the left, right, top, or bottom border of a window.
    - When shrinking or expanding a middle slot from its right border, LeapK strictly moves the divider between the middle slot and its right neighbor (`TileEdge::Right`), leaving the left neighbor completely untouched.
    - Preserves symmetric fallback behavior for keyboard navigation shortcuts (`performTilingResize`).
  - Added comprehensive automated test case in `tests/tiling_test.cpp` verifying 3-column boundary divider targeting.

## [1.6.0-beta71] - 2026-11-03

### Fixed
- **Dialog Focus & Grab Release in Tile Edit & Presets Mode**:
  - Fixed keyboard grab issue when opening `leapk-custom-layout` via `Ctrl + S`: `slotSaveCurrentTilingLayout()` now immediately releases KWin's keyboard grab (`exitTilingEditMode()`, `cancelJumpMode()`) so the save dialog receives keyboard focus and user input without blocking.
  - Added `Qt::WindowStaysOnTopHint` to `SaveLayoutDialog`, along with `showEvent` window activation, `raise()`, and auto-focusing `m_nameEdit` with full text selection.
- **KDE Custom Layout Deduplication & Dynamic Sync**:
  - Fixed bug where saving a custom preset (e.g. key `I`) caused duplicate layout cards and ghost `[C]` entries:
    - Ensured dynamic layout `kde-custom` is strictly in-memory and never written to or loaded from `~/.config/leapk/tiling_presets.json`.
    - Added comprehensive layout comparison in `refreshTilingPresetsWithKdeCustom()` against all presets (KDE JSON structure and slot geometry tolerance). If KWin's active layout already matches any user preset, `[C] KDE Custom` is omitted.
    - Cleaned transient entries when saving and deleting presets.
- **KDE Tile Editor Shortcut Migrated to `Ctrl + E`**:
  - Replaced single-key `E` shortcut with two-key combo `Ctrl + E` in both Tiling Presets mode (`Meta + T`) and Tile Edit mode (`Meta + Enter`).
  - Freed single letter `E` so user custom layouts assigned to key `E` can be activated directly without conflict.
  - Updated HUD action hint bar to display `[Ctrl+E] KDE Editor` and `[Ctrl+S] Save current`.

## [1.6.0-beta70] - 2026-11-02

### Added
- **Custom Tiling Layouts Creation & Management Engine (`leapk-custom-layout`)**:
  - Implemented dynamic creation and persistence of custom tiling presets via `Ctrl + S`:
    - `Ctrl + S` is active strictly in **Tiling Presets mode** (`Meta + T`) and **Tile Edit mode** (`Meta + Enter`), leaving standard application file-saving shortcuts completely untouched.
    - Captures the exact live layout: if tiling is currently active or edited, it calculates normalized slot geometries from active windows/slots; if not active, it captures custom divisions directly from KDE's `TileManager::rootTile()` and `kwinrc [Tiling]`.
  - Created standalone themed helper binary `leapk-custom-layout` (`customlayoutmain.cpp`):
    - Modern dark acrylic dialog matching LeapK's design system.
    - Interactive schematic tile preview widget (`TilePreviewWidget`) visualizing slot proportions and numbering.
    - Name/Label input field (`QLineEdit`) and manual single-letter trigger key input (`QLineEdit` A–Z).
    - Real-time collision status indicator informing whether the chosen key creates a new layout or safely overwrites an existing preset.
    - Standard keyboard controls: `Enter` to save, `Esc` to cancel.
  - Implemented layout deletion via **`Shift + <Letter>`**:
    - In Tiling Presets mode (`Meta + T`), pressing `Shift + <Key>` immediately removes the matching preset from `m_tilingPresets` and updates `~/.config/leapk/tiling_presets.json`.
    - Automatically plays `Dismiss` sound and refreshes HUD pill cards.
    - The `Float / Turn Off` (`X`) system action is protected against deletion.
  - Added factory preset restoration via **`Ctrl + R`** (Reset to Defaults).
  - Synchronized custom layouts with KDE KWin: activating any custom preset persists it to `kwinrc [Tiling]` and live `TileManager::rootTile()`, ensuring complete compatibility with KDE's native Shift-drag tiling and native editor (`E`).
  - Updated HUD action hint bar in Tiling Presets mode:
    `▤ TILING: Select [A–Z] • [Ctrl+S] Save • [Shift+Key] Delete • [E] KDE • [X] Float • N presets`
  - Updated Tile Edit mode hint bar to include `[Ctrl+S: Save layout]`.
  - Added unit test suite `testCustomLayoutUpsertAndDelete` in `tests/tiling_test.cpp` and CLI tests in `tests/cli_test.cpp`.

## [1.6.0-beta69] - 2026-11-02

### Added
- **Single Card/Badge Selection via Enter Key in HUD**:
  - In HUD / Jump Mode, when only one card/badge is available to choose (`m_jumpLetterToWindow.size() == 1`), pressing `Enter` (`Key_Return` or `Key_Enter`) now automatically selects and activates that window.
  - Specifically supports `Meta + U` (Audio Filter mode): when only one window is playing audio, pressing `Enter` immediately switches to that window without needing to press its jump badge letter.
  - Also applies across all HUD scopes (e.g. SubLeap with 1 candidate, filtered workspace views, Stash view, SuperJump) whenever a single target window is present.
  - Unified jump activation into helper method `activateJumpTargetWindow()`, handling desktop/workspace switching, unminimizing, tiling swap, cursor warping, and clean jump mode exit.
  - Updated the HUD action hint bar (`renderHudActionHintBar`) to dynamically display `Enter / Key` highlighted in green when `candidateCount == 1`, visually guiding the user that `Enter` selects the single available window.
  - Added unit test assertions in `tests/badge_test.cpp` verifying hint bar rendering for single candidate HUD modes.

## [1.6.0-beta68] - 2026-11-02

### Fixed
- **Tiling Resize Window Pop-out & Off-Screen Overflow Elimination**:
  - Fixed a critical bug in tiling resize where shrinking the master window or repeatedly pressing resize keys (`H`/`L`/`J`/`K`) caused stack windows to change size and stick out beyond the screen edge ("okna z stack zmieniają rozmiar i wystają poza ekran").
  - Diagnosed and resolved the underlying KWin Wayland conflict: KWin's internal `Window::checkWorkspacePosition` / `Window::ensureSpecialStateGeometry` unconditionally snaps windows associated with a native `KWin::Tile` back to `tile->windowGeometry().x()`. When LeapK's co-operative resizing shifted stack windows to the left while enlarging their width, KWin forced their X coordinate back to the static native tile position (e.g. `1153px`) while applying the new enlarged width (e.g. `950px`), pushing the window 183px past the edge of a 1920px screen (`1153 + 950 = 2103px`).
  - Decoupled LeapK tiled windows from KWin's native `Tile` hierarchy in `applyWindowToTilingSlot`: cleanly detaches windows from KWin's quick-tiling state machine (`tile->unmanage(win)`, `win->setTileCompatibility(nullptr)`, `win->requestTile(nullptr)`), giving LeapK full control over window geometries without KWin snapping them back or triggering premature unmanage `geometryRestore()`.
  - Cleanly decoupled LeapK's real-time divider resizing engine: `handleTiledWindowResized` and `syncActiveTilingSlotsToKdeTiles` now directly read and manage actual window frame geometries rather than reading stale `leafTiles[i]->windowGeometry()`.
  - Normalized vertical child heights (`h1 + h2 == 1.0`) in `toKdeJson()` inside `tilinghelper.h` for container stacks (`master-stack` and `grid-4`), ensuring KDE's underlying configuration remains mathematically consistent.
  - Added automated test suite `testMasterStackRepeatedResize` in `tests/tiling_test.cpp` verifying that repeated resizing in `master-stack` strictly anchors outer screen boundaries, preserves gaps, and never pushes stack windows outside the screen.

## [1.6.0-beta67] - 2026-11-02

### Refactored
- **Core Architecture & Subsystem Modularization**:
  - Refactored the monolithic `leapk.cpp` (12,662 lines) into 8 dedicated, high-cohesion subsystems:
    - `leapk.cpp` (~1,900 lines): Core plugin lifecycle, configuration management, KWin window event slots, popup filtering, and DBus interface.
    - `leapk_shaders.cpp` (~1,000 lines): GLSL shaders generation & compilation (glow, outline, audio reactive), vertex geometry generation, and outline rendering.
    - `leapk_color.cpp` (~580 lines): Real-time ambilight screen edge sampling, color cycling, and visual animations (breathe, glitch, neon, flash).
    - `leapk_paint.cpp` (~1,290 lines): KWin screen and window paint passes (`prePaintScreen`, `paintScreen`, `paintWindow`, `drawWindow`, and live window thumbnail capture).
    - `leapk_tiling.cpp` (~1,240 lines): Tiling engine, slot management, preset activation, KWin native leaf tile synchronization, interactive keyboard resizing/swapping, and edit overlays.
    - `leapk_hud.cpp` (~2,460 lines): HUD layout computation, dock/pill geometry calculation, badge letters rendering, hint bars, mode headers, and meta overlays.
    - `leapk_input.cpp` (~2,300 lines): Modal keyboard event grabber (`grabbedKeyboardEvent`), mouse interaction, and mode state machines (Jump, SubLeap, SuperJump, CloseWindow, Pin, Search, Audio filter).
    - `leapk_workspaces.cpp` (~1,910 lines): Smart Workspaces, virtual desktop isolation, window tagging & marks, persistent marks, pin/stash management, and history navigation.
    - `leapk_internal.h`: Unified internal header with forward declarations and includes for all effect translation units.
  - Enabled multi-core parallel compilation in CMake for `leapk.so`, dramatically speeding up build times.
  - Updated build ID hash calculation in `CMakeLists.txt` to cover all subsystem translation units.
  - All 30/30 unit tests pass.

## [1.6.0-beta66] - 2026-11-02

### Fixed
- **Tiling Resize Occlusion and Full-Screen Stretch Fix**:
  - Fixed an issue in Tiling Edit mode where resizing tiles (e.g. `J`/`L` or arrow keys) broke the layout, causing the active window to expand across the full screen (`1904x1032`) and occlude other tiles.
  - Added strict guard checking `leafTiles.size() == m_activeTilingSlots.size()` in `applyWindowToTilingSlot`, `handleTiledWindowResized`, and `syncActiveTilingSlotsToKdeTiles`. When KWin's native tiling configuration has a tile count mismatch (such as a single full-screen root tile), windows are not bound to mismatched native leaf tiles, preventing KWin from forcing windows into 100% full-screen geometry.
  - In `performTilingResize`, removed the premature call to `syncActiveTilingSlotsToKdeTiles()` which was overwriting resized slot geometries with stale native tile bounds.
  - Synchronized resized slot proportions in real-time to `m_activeTilingPreset.tileSlots` relative rects, and updated `toKdeJson()` across presets (`columns-2`, `columns-3`, `master-stack`, `grid-4`) to serialize actual relative proportions instead of hardcoded defaults.
  - On exiting Tiling Edit mode (`exitTilingEditMode`), updated layout geometry is persisted into `kwinrc` under `[Tiling]` and reloaded via `tileMgr->readSettings(rootTile)`.
  - Improved keyboard controls and hints in Tiling Edit mode:
    - Added `I` key for resizing Up.
    - Added automatic fallback for `J` to resize Left when the active layout is purely horizontal (e.g. 2 or 3 columns), accommodating both Vim (`H/J/K/L`) and directional (`I/J/K/L`) key habits.
    - Updated bottom overlay hint bar: `[H/←: ◀ Left  L/→: ▶ Right  K/↑: ▲ Up  J/↓: ▼ Down]  •  [1-9: Step]  •  [Letters: Swap]  •  [Enter / Esc: Done]`.

## [1.6.0-beta65] - 2026-11-02

### Fixed
- **Minimized and Shrunk Windows in LeapK HUD**:
  - Fixed an issue where minimized windows on the current desktop were excluded from HUD candidate matching when `JumpTargets` was set to 0. Now minimized windows are always retained and assigned jump letters, displaying as cards in the HUD dock.
  - Jumping or clicking on a minimized window card in the HUD automatically restores and unminimizes it (`candidate->setMinimized(false)`), bringing it to the foreground with proper focus.
  - Added dedicated location indicator `"Minimized"` in `getWindowLocationString` so minimized window cards in the HUD clearly show their status.
  - Fixed Standard workspace window count (`standardCount`) to include minimized windows on the current desktop rather than dropping them when minimized.
  - Reduced minimum window size eligibility cutoff in `isEligibleTargetWindow` from 30px to 10px so smaller or shrunk floating windows are never prematurely filtered out.

## [1.6.0-beta64] - 2026-11-02

### Added
- **Audio Window Frame Shader Targeting Rules**:
  - Added configurable targeting mode for audio reactive window frames (`AudioWindowFrameShaderMode`):
    - `always` ("Zawsze (aktywne i nieaktywne)"): Shader applies to window frames of audio-playing windows whether active or inactive.
    - `active` ("Tylko gdy jest aktywne"): Shader applies to the window frame only when the window is currently active.
    - `inactive` ("Tylko gdy jest nieaktywne"): Shader applies to the window frame only when the window is inactive (e.g. background music / video playback).
  - Added rendering pass in `paintScreen` for inactive audio window frames, allowing background windows playing audio to receive their real-time GLSL outline/glow effects even when not explicitly marked, pinned, or in recent history.
  - Configured `needsContinuousRepaint` to trigger smooth 60+ FPS shader animation for inactive audio window frames when playing sound in background.
- **HUD Audio Reactive Shader Option**:
  - Added `AudioHudShaderEnabled` option to apply the custom shader effect directly to audio-playing windows in HUD.
  - Integrated GPU-accelerated glow pass (behind HUD card) and outline pass (on card borders) according to the shader target (`glow`, `outline`, or `both`).
  - Added continuous repaint wakeup when HUD is open and any window is playing audio with HUD audio shader enabled.
  - Added audio indicator color border tint fallback in `renderOffscreenTargetPill` and `BadgeConfig`.
- **Settings UI & Presets/Packs Integration**:
  - Added combo box `comboAudioWindowFrameMode` and checkbox `kcfg_AudioHudShaderEnabled` under Audio Reactive Shader Rules in LeapK Settings.
  - Added synchronization in presets (`presethelper.h`), preset activation categories (`presetactivationdialog.h`), pack manifest (`packhelper.h`), installer defaults (`install-leapk.sh`), and uninstaller cleanup (`uninstall-leapk.sh`).
  - Added unit test coverage for frame mode rules and HUD audio shader in `tests/audiotracker_test.cpp`, `tests/preset_test.cpp`, and `tests/pack_test.cpp`. All 30/30 CTests pass.

## [1.6.0-beta63] - 2026-11-02

### Added
- **Dedicated Tab Switching Shortcuts in LeapK Manager**:
  - Added dedicated keyboard shortcuts to switch and cycle between tabs (`Workspaces` vs `Markers & Groups`):
    - `[Ctrl+Tab]` / `[Ctrl+Shift+Tab]` / `[Ctrl+Backtab]`: cycle tabs forward / backward.
    - `[Ctrl+1]` / `[Alt+1]` / `[Ctrl+PageUp]`: direct jump to Workspaces tab.
    - `[Ctrl+2]` / `[Alt+2]` / `[Ctrl+PageDown]`: direct jump to Markers & Groups tab.
    - `[[]` and `[]]`: quick bracket switching when not typing in text fields.
  - Tab switcher buttons in header now explicitly display their shortcut badges (`🗂 Workspaces [Ctrl+1]` and `🏷 Markers & Groups [Ctrl+2]`) with comprehensive tooltips.
  - Footer shortcut hint strips updated to document `[Ctrl+Tab / Ctrl+1/2] Switch Tab  •  [Tab] Jump Controls`.
- **Comprehensive Control Tab Navigation with Prominent Focus Outline and Enter Activation**:
  - Removed plain `Tab` interception from tab switching so that `Tab` and `Shift+Tab` smoothly navigate focus across all interactive controls.
  - Enabled `Qt::StrongFocus` on all buttons, inputs, and cards:
    - Tab buttons (`Workspaces`, `Markers & Groups`), Close button (`[Esc] Close`).
    - Scope switcher buttons (`Application Scope`, `Single Window Scope`).
    - Workspace cards and marker rule cards, including their inline action buttons (`✏️ Edit`, `❌ Delete`, `● Active / ○ Off`).
    - Creation and action buttons (`Create with window`, `Create Empty Workspace`, `Unassign`, `Edit`, `Delete`, `Cancel`, `Done`).
  - Added high-contrast, prominent focus outline styling across all controls:
    - 2px-2.5px bright solid borders with subtle luminous background highlights (`border: 2px solid #ffffff; outline: none;` on accent widgets, `border: 2px solid #52a9ff;` on neutral widgets, `border: 2px solid #ff7675;` on destructive widgets).
    - Focused cards prominently light up with `2.5px solid #ffffff` borders, and automatically scroll into view in scroll areas.
  - Direct `Enter` / `Return` and `Space` activation on focused controls:
    - Pressing `Enter` / `Return` (or `Space`) while any button has focus calls `btn->animateClick()`, triggering its native action.
    - Pressing `Enter` / `Return` (or `Space`) while a card has focus toggles its assignment / enabled state.
    - When no specific button or card has focus, standard global key events (`Enter` for Done / Edit, `Space` for assignment, `1..9` for direct selection) continue to function seamlessly.
  - Harmonized subdialogs (`CloseWorkspaceDialog`, `EditWorkspaceDialog`, `EditMarkerRuleDialog`) with full `Qt::StrongFocus`, focus outlines, and Enter activation.

## [1.6.0-beta62] - 2026-11-01

### Added
- **Unified Manager Application (`UnifiedManagerDialog`)**:
  - Consolidated Workspace Manager and Marker & Group Manager into a single, cohesive, dual-tabbed application:
    - **Tab 1: Workspaces (`🗂 Workspaces`)**: Manage virtual workspaces, window/application scope assignment, stash, and creation.
    - **Tab 2: Markers & Groups (`🏷 Markers & Groups`)**: Manage persistent letter marks, window rules, criteria, and live open windows.
  - Seamless tab switching via clicking tabs, pressing `Tab` / `Ctrl+Tab`, or pressing `Ctrl+1` / `Ctrl+2`.
  - Both helper binaries (`leapk-workspace-manager` and `leapk-marker-manager`) now launch the unified interface preselected to their respective tabs, maintaining full backwards compatibility with all CLI options, scripts, and KWin HUD shortcuts (`Meta+W` and `Meta+M`).
  - Dynamic footer shortcut hint strip adapting to the active tab's capabilities and keys.
- **Wider Manager Window Geometry**:
  - Expanded dialog dimensions to `850px x 650px` (with `800px x 560px` minimum bounds), providing generous horizontal breathing room for marker criteria, titles, and live matching open window indicators.

### Changed
- **Workspace Manager Edit Shortcut Changed to [E]**:
  - Changed workspace edit shortcut from `F2` to `[E]` (with `[R]` and legacy `[F2]` retained as aliases), harmonizing edit semantics across both Workspaces and Markers (`[E]` edits the selected item in both tabs).
  - Empty workspace creation shortcut updated to `[C]` ("✨ [C] Create Empty Workspace", with `Shift+N` also supported).
- **Marker Manager Search Defocus on Escape**:
  - Pressing `Esc` while the search input (`m_searchEdit`) has focus now clears focus and returns control to the manager without closing the dialog. A subsequent `Esc` closes the window as expected.
- **Purge of Legacy Compatibility Targets and Names**:
  - Removed all obsolete `kleap-*` and `active-window-outline-*` custom targets from `CMakeLists.txt`.
  - Cleaned up executable lookup paths in `leapk.cpp` to strictly reference native `leapk-*` binaries.
  - Cleaned up icon aliases in `resources.qrc` and stale symlinks in `install-leapk.sh`.

## [1.6.0-beta61] - 2026-11-01

### Added
- **Audio Tracking & Playing Window Customization in Settings (`leapk-settings` / KCM)**:
  - Added dedicated Audio configuration controls in LeapK Settings:
    - **HUD Audio Badge**: configurable indicator style (`speaker_icon`, `soundwave`, `minimal_dot`), pulse animation toggle, pulse speed (0.2x – 3.0x), pulse intensity (0.0x – 2.5x), automatic vs custom badge color picker.
    - **Window Audio Badge**: indicator style (`speaker_icon`, `soundwave`, `minimal_dot`), pulse animation toggle, pulse speed, pulse intensity, and badge scale (0.7x – 2.0x).
    - **Audio-Triggered GLSL Shader Rule**: automatically activate a chosen GLSL shader effect (e.g. Neon Pulsar, Audio Wave, Glow) around any window currently outputting audio via PipeWire/PulseAudio.
  - Dynamically binds and executes shader passes for audio windows in `leapk.cpp` (`m_audioGlowShader` and `m_audioOutlineShader`) with continuous animation rendering.
- **Architectural Consolidation & LeapK Pack Format (`.leapkpack`)**:
  - Clear architectural separation between:
    - **Themes**: Visual appearance, shape geometry, glassmorphism tokens, HUD cards, keycaps, and colors.
    - **Presets**: Operational behaviors, ergonomics, auto-hide timings, window recency, and tiling layout preferences.
    - **Shaders**: GPU GLSL fragment programs for window borders and glow rendering.
    - **LeapK Pack (`.leapkpack`)**: Complete bundle unifying Themes, Presets, Shaders, and KDE Global Shortcuts (`KGlobalAccel`), enabling 1-click sharing, website distribution, and installation.
  - Added `LeapKPack` engine (`packhelper.h`) with JSON manifest format, bundle serialization/deserialization, integrity validation, and atomic installer.
  - Added settings UI actions: **"Install Pack (.leapkpack)…"** and **"Export Pack…"** in the Themes tab of `leapk-settings`.
  - Added CLI and desktop file support: `leapk-settings --install-pack <file.leapkpack>` and direct MIME association `application/x-leapkpack`.
  - Added comprehensive test suite `outline-pack-test` covering pack serialization, export, import, and component installation.

### Changed
- **Tray Icon Streamlining (`tray.cpp`)**:
  - Removed cluttered `📁 Presets` and `✨ Shader Effects (GLSL)` submenus from the system tray menu.
  - System tray icon is now lightweight, fast, and cleanly focused on quick utilities (Toggle outline, Jump to window, Workspace Manager, Marker Manager, Cheat Sheet, and LeapK Settings).

## [1.6.0-beta60] - 2026-11-01

### Added
- **Global Visual Theme Integration Across Auxiliary Dialogs**:
  - The LeapK Theme Engine (`retro-terminal`, `cyberpunk-neon`, `nordic-frost`, `default-breeze`, and custom user themes) now styles all auxiliary windows and dialogs:
    - **Workspace Manager (`leapk-workspace-manager`)**:
      - `MainWorkspaceFrame`: adopts theme frame shape (`sharp`, `chamfered`, `pill`, `rounded_rect`), background surface opacity, frame glow, border color, and top accent line.
      - `WorkspaceCard`: styled with theme card background, borders, card corner radius, dynamic hover highlighting, and keycap accent color matching active theme.
      - `EditWorkspaceDialog` & `CloseWorkspaceDialog`: styled with active theme colors, themed buttons, and inputs.
      - Scope switcher buttons (`App` vs `Single Window`) and action buttons adapt to theme accent and card styles.
    - **Marker & Group Manager (`leapk-marker-manager`)**:
      - `MainMarkerFrame`: adopts theme frame shape, acrylic glass surface, glow, and border.
      - `MarkerRuleCard`: themed cards with adaptive corner radius, active phosphor/neon accent colors for active state, dashed frames for disabled state, and themed badges.
      - `EditMarkerRuleDialog`: dialog background, inputs, combo boxes, and save/cancel buttons inherit theme palette and corner radius.
      - Search bar and bottom action buttons dynamically match theme accent color.
    - **Custom Marker & Color Picker (`leapk-custom-marker` / `colorpicker`)**:
      - `MainMarkerFrame`: paints themed outer frame matching HUD style.
      - `MarkerOptionCard`: adopts themed card background, borders, and corner radius.
      - `PatternFilterDialog`: adopts themed acrylic surface, accent border, and themed inputs.
      - Live preview card, label inputs, and navigation buttons adapt to theme accent colors.
  - **Shared Theme Engine Helpers in `themehelper.h`**:
    - Added convenience methods to `LeapKTheme::Theme`: `primaryAccentColor()`, `secondaryAccentColor()`, `surfaceBackgroundColor()`, `cardBackgroundColor()`, `cardBorderColor()`, `cardCornerRadius()`, `textPrimaryColor()`, and `textSecondaryColor()`.
    - Added `LeapKTheme::paintThemedWindowFrame()` for painting unified window frames supporting shapes (`rounded_rect`, `sharp`, `pill`, `chamfered`), glow, borders, and accent stripes.
    - Added `LeapKTheme::generateDialogStyleSheet()` for unified scrollbars and input field styling.
    - Added `LeapKTheme::loadActiveTheme(overrideThemeId)` reading kwinrc configuration with active theme knob overrides.
  - **KWin Effect Integration & CLI Passing**:
    - `leapk.cpp` automatically passes `--theme <activeThemeId>` when spawning helper processes for workspace manager, marker manager, and custom marker dialogs, ensuring instant theme synchronization.
    - Added comprehensive unit tests in `tests/theme_test.cpp` and `tests/cli_test.cpp`.

## [1.6.0-beta59] - 2026-11-01

### Fixed
- **Marker & Group Manager Close Key Standardized to Esc**:
  - In Marker & Group Manager (`leapk-marker-manager`), pressing `Esc` now closes the window directly, aligning with standard desktop dialog conventions.
  - Removed single-key `Q` closing shortcut to prevent unintended window dismissal (standard window closing shortcuts `Ctrl+W` and `Ctrl+Q` remain active).
  - Search field focus is cleanly dedicated to `[/]` (Slash) and `Ctrl+F` instead of reusing `Esc`.
  - Updated all dialog UI hints:
    - Close button updated to `[Esc] Close` (tooltip: `Close Marker Manager [Esc]`).
    - Bottom shortcut hint strip updated with `[/] Search` and `[Esc] Close`.
    - Search input placeholder updated to indicate `[/]` to search and `[Esc]` to close.
    - Filter empty state message updated to `[Esc] to close`.

## [1.6.0-beta58] - 2026-11-01

### Changed
- **High-Contrast Prominent Dashed Frame & Emerald Stash Styling for Stashed Badges**:
  - Significantly enhanced visual distinction of stashed window badges and HUD dock pills so they stand out clearly:
    - **Bold Dashed Outer Frame**: Stashed HUD pills and on-screen badges now feature a prominent `2.0 * scale` (pill) and `2.4 * scale` (jump badge) dashed stroke with crisp `{5, 3}` pattern.
    - **Luminous Emerald Stash Theme**: Unmarked stashed pills and badges feature vibrant Emerald Green (`#2ecc71` / `rgba(46, 204, 113, 235)`) borders and inner strokes instead of dull washed-out grey.
    - **Stash Container Background**: Tinted pill backgrounds with a subtle dark emerald glass tone (`#102218`) and rich dark emerald letter badge container (`#183826`).
    - **Vibrant Header Text**: Location text ("📦 Stash" / "📦 Schowek") is rendered in bright emerald green (`QColor(46, 204, 113)`).
    - **Stash Icon Badge Prefix**: On-screen window badges and custom labels for stashed windows are automatically tagged with the `📦` icon prefix.
    - **Dashed Thumbnail Frame**: Stashed window thumbnails in HUD pills feature a matching emerald dashed border (`{4, 2}`).
  - Updated unit tests in `tests/badge_test.cpp` verifying unmarked and marked stashed badges and HUD pills.

## [1.6.0-beta57] - 2026-11-01

### Changed
- **Auto-Dismiss Meta Cheatsheet Overlay on Recent Windows Traversal (`Meta + Space Space`)**:
  - Automatically hides the centered Meta-hold shortcut cheatsheet overlay (`renderHudMetaOverlay`) as soon as recent windows traversal begins on the first `Space` press (`m_metaSpaceTraverseActive == true`).
  - Ensures the user has an unobstructed view of the windows being cycled through (Alt+Tab style) while keeping Meta held.
  - The cheatsheet remains hidden across subsequent `Space` / `Shift+Space` taps and resets cleanly upon releasing Meta or exiting Leap mode.
  - Updated unit test in `tests/history_test.cpp` verifying cheatsheet visibility transitions during traversal.

## [1.6.0-beta56] - 2026-11-01

### Fixed
- **Eliminated Ghost Workspace View and Fixed Workspace Navigation Count**:
  - Fixed an issue where cycling through workspaces included an invisible 3rd position (with all workspace tabs unselected) when only "Standard" and "All" workspaces existed.
  - Root cause: `totalViews` was computed as `3 + m_workspaces.size()` (a leftover from when Stash was an in-band tab before becoming a dedicated drawer mode), instead of `2 + m_workspaces.size()`. With 0 custom workspaces, `totalViews` was 3, cycling through index 0 (Standard), index 1 (All), and phantom index 2 where neither tab matched `m_activeHudView`.
  - Added `computeTotalWorkspaceViews(customCount)` and `workspaceIdForHudView(index, workspaces)` in `workspacehelper.h` to strictly bound total views to `2 + customWorkspaceCount`.
  - Updated `refreshWorkspaces()`, `switchToWorkspaceView()`, and `slotCycleHudView()` in `leapk.cpp` to use the unified view count calculation and automatically sanitize / persist out-of-bounds `ActiveHudView` values to `kwinrc`.
  - Added unit test `testWorkspaceViewsAndCycling` in `tests/workspace_test.cpp` verifying view bounds and bidirectional cycling for both 0 and multiple custom workspaces.

## [1.6.0-beta55] - 2026-11-01

### Fixed
- **Restored Animated Recency Marker Line Between HUD Badges**:
  - Fixed an issue where the animated vertical glowing recency marker line (`renderHudActiveMarkerLine`) between HUD badges disappeared when cycling through windows via `Shift + ,` / `Shift + .` or `Meta + Space`.
  - Root cause: In `v1.6.0-beta49`, the active window was unconditionally added to `offscreenTargets` in Leap mode. `computeActiveMarkerRectForTargets()` immediately returned `QRectF()` when the active window was found inside `targets`, permanently suppressing the line between badges.
  - In `badgehelper.h`, reverted `cand.isActiveWindow` from unconditionally forcing `res.addToHud = true` when `BadgeHudIncludeAll` is false.
  - In `leapk.cpp`, restored conditional inclusion of offscreen targets in Mode A and Mode B: on-screen visible windows are not added to `offscreenTargets` when `BadgeHudIncludeAll` is false unless special modes (Pin, Stash, Close) are active.
  - Connected `traverseRecentWindows` (`Meta + Space`) to `m_hudSelectAnimTimer` and `m_hudSelectAnimActive` so the recency marker line animates smoothly during recent windows traversal.
  - Set default `BadgeHudIncludeAll` to `false` across KConfig schema (`leapkconfig.kcfg`), settings UI (`leapk_config.cpp`), preset defaults (`presethelper.h`), and installer script (`install-leapk.sh`).
  - Updated unit tests in `tests/badge_test.cpp` verifying target candidate distribution when `hudIncludeAll = false`.

## [1.6.0-beta54] - 2026-11-01

### Added
- **Alt+Tab Style Recent Window Traversing via `Hold Meta + Space` / `Shift+Meta+Space`**:
  - Hold `Meta` and tap `Space` to cycle forward circularly through recent windows.
  - Hold `Meta` and tap `Shift+Space` (or `Shift+Meta+Space`) to cycle backward.
  - Releasing `Meta` immediately commits the focused window and exits Leap mode with cursor warping and land sound.
  - Pressing `Return` or `Enter` immediately commits the selection; pressing `Escape` restores the original window and cancels traversal.
  - Added `stepHistoryIndex` in `historyhelper.h` with circular wrapping and verified in unit tests (`tests/history_test.cpp`).
- **Comprehensive Shortcut Cheat Sheet Integration**:
  - Added `Meta+Space: Traverse Recent` to the HUD Meta-hold cheat sheet overlay (`renderHudMetaOverlayImage`), featuring active green indicator during traversal.
  - Added `Hold Meta+␣: Traverse recent` to the HUD action hint bar (`renderHudActionHintBar`).
  - Added shortcut documentation to the LeapK system tray cheat sheet dialog (`tray.cpp`) and `README.md`.

## [1.6.0-beta53] - 2026-11-01

### Fixed
- **Seamless KDE KWin Tile Integration & Bidirectional Gap / Border Resizing**:
  - Re-integrated tiled windows with KWin's native `Tile` hierarchy via `applyWindowToTilingSlot(ew, geo, targetTile)` and `getLiveKdeLeafTiles()`:
    - Automatically links tiled windows to KWin's Wayland `Tile` state machine (`targetTile->manage(win)`).
    - Enables KDE's native gap resizing: hovering over the gap between two tiled windows reveals the resize handle and dragging it resizes both tiles and windows simultaneously.
    - Window borders between tiles now trigger co-operative tile resizing instead of detaching as isolated floating windows.
  - Connected `EffectWindow::windowStepUserMovedResized` and upgraded `slotWindowFinishUserMovedResized`:
    - Synchronizes live window and tile geometries during interactive dragging in real-time.
    - On release, persists updated tile proportions into `kwinrc` via `TileManager::saveSettings()` and synchronizes custom preset backups.
  - Implemented `calculateDividerMove` and `localOnly` component isolation in `tilinghelper.h`:
    - Prevents horizontal divider resizing in one column (e.g. dragging between Slot 3 and Slot 4 in the right column) from inappropriately altering dividers in adjacent columns (Slot 0 and Slot 1 in the left column).
    - Added comprehensive unit test in `tests/tiling_test.cpp` verifying edge movement detection and column-isolated divider resizing for the 5-slot layout.

## [1.6.0-beta52] - 2026-11-01

### Fixed
- **Eliminated KWin Wayland Tile Conflict and Fixed Window Resizing**:
  - Unified all window placement into `applyWindowToTilingSlot(EffectWindow *ew, const QRectF &geo)`:
    - Automatically detaches windows from KWin's internal Wayland `Tile` state machine (`tile->unmanage(win)`, `win->setTileCompatibility(nullptr)`, `win->requestTile(nullptr)`).
    - Prevents KWin Wayland from rejecting `moveResize()` calls, which previously left windows at full screen width (e.g. 1914px extending off-screen to 2877px) or height (e.g. 1042px extending down past the screen).
    - Restores maximized windows (`setMaximize(false, false)`) and evaluates window rules before resizing into exact tile slots.
  - Removed all obsolete `leafTiles[slotIdx]->manage(w->window())` calls across `activateTilingPreset`, `slotWindowAdded`, `slotToggleActiveWindowFloating`, `swapWindowIntoTilingSlot`, and `performTileSlotSwap`.
  - Removed erroneous `leafTiles[i]->windowGeometry()` override that was corrupting slot geometries with stale or asynchronous KWin tile coordinates.
- **Support for All 5 Slots and Arbitrary Asymmetric Custom Layouts**:
  - Added 4th priority pass during window collection in `activateTilingPreset`: if open visible windows are fewer than slot count, remaining slots are populated by unminimizing eligible windows on the current desktop/workspace.
  - Improved recursive sibling layout advance in `parseKdeTileJsonInternal` by computing `maxRight` / `maxBottom` across all slots in a split branch and snapping boundary edges to eliminate 1px rounding gaps.
  - Added unit test in `tests/tiling_test.cpp` verifying the exact 5-slot layout (3 left, 2 right) with workArea boundaries and priority order.

## [1.6.0-beta51] - 2026-11-01

### Fixed
- **Robust Support for Asymmetric Splits and Custom Tiling in KDE Custom (`kde-custom`)**:
  - Implemented recursive parser `parseKdeTileJson` in `tilinghelper.h` mirroring KWin 6's exact tiling layout engine:
    - Recursively parses nested horizontal, vertical, and floating layouts without flattening them.
    - Accurately computes normalized `[0.0, 1.0]` coordinates for asymmetric splits (e.g., 1 on right with 3 on left, 1 on left with 3 on right, or complex nested sub-splits).
  - Preserved genuine custom tile JSON (`kdePreset.kdeJson`) across LeapK preset activations and serialization so custom setups in `kwinrc` are never overwritten or lost when experimenting with built-in presets.
  - Automatically backs up and restores the user's custom layout to/from `kwinrc` (`[Tiling]`) and `Effect-leapk` settings upon activating `kde-custom`.
- **Intelligent Master/Priority Window-to-Slot Allocation**:
  - Implemented `computeSlotPriorityOrder` in `tilinghelper.h`:
    - Automatically ranks tile slots by area and geometric position.
    - In asymmetric layouts (such as 1-right-3-left or Master & Stack), the largest slot (Master) is prioritized for the active window.
    - When there are fewer windows than slots (e.g. 2 windows in a 4-slot layout), windows are placed across primary zones (e.g. 1 on right, 1 on left) instead of squishing all windows into tiny slices on one side and leaving the other side empty.
- **Full Support for Floating Tiles and Floating Windows**:
  - Added support for KWin 6 floating tile layouts (`layoutDirection: "floating"`, explicit `x, y, width, height` geometries).
  - Added dedicated window-level floating management (`m_floatingWindows`):
    - Floating windows are excluded from being swallowed by tile slots during preset activation or window creation.
    - Floating windows remain raised above tiled slot backgrounds so they are never buried.
    - Added global shortcut `Meta+Shift+F` ("LeapK: Toggle Window Float/Tile") and method `slotToggleActiveWindowFloating()` to seamlessly toggle any active window between tiled and floating mode.
  - Added comprehensive unit tests in `tests/tiling_test.cpp` (`testParseKdeTileJson`, `testComputeSlotPriorityOrder`).

## [1.6.0-beta50] - 2026-11-01

### Changed
- **Swapped Layout for Bottom HUD Position (Badges on Top, Navigation Bar on Bottom)**:
  - When HUD is set to the Bottom position (`hudPos == 0`), inverted the vertical hierarchy into a true dock paradigm:
    - **Window Cards / Badges on Top**: Offscreen jump target pills and active marker line are positioned on top, floating above the dock bars and extending upwards towards the center of the screen as rows wrap.
    - **Action Hint Bar in Middle**: When hints are enabled (`hudShowHints`), the hint bar sits directly below the window cards (with a 12px margin) and 8px above the bottom navigation header.
    - **Navigation Bar at Bottom**: The HUD header bar (workspace tabs, stash tabs, search indicator, close button) is pinned directly against the screen's bottom margin (`screenRect.bottom() - headerH - 12.0`).
  - Extracted `ensureHudHintBarTexture` and introduced `computeHudEffectiveMargin` to consistently calculate exact bottom margins across `computeHudPillRects`, `renderOffscreenJumpTargets`, and `renderTilingPresetPills`.
  - Maintained top-anchored layout for Top HUD (`hudPos == 1`) and side-anchored layouts (`hudPos == 2, 3`) without regressions.
  - Added unit test `testBottomHudSwappedLayout` in `tests/hudlayout_test.cpp`.

## [1.6.0-beta49] - 2026-11-01

### Fixed
- **Ensure Last Badge in HUD Dock is Always Visible**:
  - Fixed an issue where the active/origin window was omitted from the HUD dock targets list, causing the last badge in the recency timeline to appear missing/invisible even though it was selectable with `Shift + .`.
  - In both Mode A (grouped cards) and Mode B (direct cards), the active window (`w`, `m_activeWindow`, `hudRefWindow`) is now unconditionally included in `offscreenTargets` so it receives its full pill card in the HUD dock with prominent active highlight styling (`themeBgActive`, `themeBorderActive`).
  - When cycling badges with `Shift + .` or `Shift + ,`, every window in the sequence (including the last badge) is consistently rendered as a full, visible, navigable card in the dock.
  - Set default of `BadgeHudIncludeAll` to `true` across `leapkconfig.kcfg`, `install-leapk.sh`, `presethelper.h`, and `leapk.cpp`.
  - Added unit test in `tests/badge_test.cpp`.

## [1.6.0-beta48] - 2026-11-01

### Added
- **Dismiss Badge Previews by Clicking on Badges or HUD**:
  - **Click-to-Dismiss on Preview Badges**: When "Preview Window Badges" or "Preview HUD Badges" is enabled, clicking with the mouse on any preview element (floating window badges, HUD dock pills, HUD header, hint bar, close button, or dock bounding box) immediately dismisses the live preview and plays the themed dismiss sound.
  - **Interactive Hover Cursor**: Hovering the cursor over any preview badge or preview HUD element dynamically displays `Qt::PointingHandCursor` indicating it can be clicked to dismiss.
  - **Transparent Input Handling**: Other windows and desktop controls—especially the settings dialog itself (sliders, tabs, input fields)—retain full normal interactivity without interference or global event capturing.
  - **D-Bus UI Synchronization**: LeapK emits `badgePreviewChanged(bool active)` over session D-Bus. The settings dialog (`ActiveWindowOutlineConfig`) receives this signal and immediately unchecks the preview checkboxes (`check_previewWindowBadges` and `check_previewHudBadges`) with `QSignalBlocker` to keep the UI perfectly synchronized.
  - Added unit test `testBadgePreviewDismissHitTesting` in `tests/badge_test.cpp`.

## [1.6.0-beta47] - 2026-11-01

### Added
- **Smooth Selection Animation for HUD Marker Line and On-Screen Badges**:
  - **Fluid Active Marker Transition**: When cycling badges in HUD (`Shift + ,` / `Shift + .`), switching active windows, or selecting targets:
    - Moving between on-screen windows (Marker -> Marker): The vertical indicator line glides smoothly between positions using a cubic ease-out curve (`~250ms`) with an elegant fading ghost trail at the previous position.
    - Moving from an on-screen window to a dock pill (Marker -> Dock): The previous marker line smoothly and gradually fades out its glow & opacity (`1.0 - t`) instead of vanishing abruptly.
    - Moving from a dock pill to an on-screen window (Dock -> Marker): The new marker line fades in smoothly (`t`).
  - **Gradual Fade-Out / Transition for Active Window Badges**:
    - When leaving an active on-screen window, its badge active highlight / color smoothly fades down ("powoli zanika") back to normal background color over 250ms.
    - If window badges are disabled except for active windows, the badge of the previous window smoothly fades out its opacity to 0 (`1.0 - t`) rather than disappearing in 1 frame.
    - The newly active window's badge smoothly rises into vibrant active highlight (`t`).
  - **60Hz/144Hz Refresh Driven with Zero Overhead**: Driven by LeapK's `m_animationTimer` via `effects->addRepaintFull()` during the 250ms transition, using OpenGL texture alpha modulation (`GLShader::Vec4Uniform::ModulationConstant`), stopping immediately when animation finishes.
  - Added unit test `testHudMarkerAndBadgeSelectionAnimation` in `tests/hudlayout_test.cpp`.

## [1.6.0-beta46] - 2026-10-31

### Added
- **Active Badge Indication and Recency Insertion Marker in HUD Dock**:
  - **Dock Pill Active Highlighting**: When the active window is present in the HUD dock pills (`activeDockIndex >= 0`), its pill card is rendered with prominent active styling: vibrant `themeBorderActive` stroke (3.0px width), `themeBgActive` card background, and active badge square background (`themeBorderActive`) for immediate visual identification.
  - **Vertical Recency Insertion Marker Line**: When `BadgeHudIncludeAll` is disabled and the currently active window is on-screen (with its badge floating on the window itself and omitted from the dock pills), a sleek vertical glowing marker line (`renderHudActiveMarkerLine`) is rendered between the dock badges at the exact insertion point where the active window belongs in the recency timeline (from oldest on the left to youngest on the right, respecting pinned and stash partitioning).
  - Added geometric calculation `OutlineBadge::computeActiveMarkerRect()` and unified collision handling in `computeAllHudCollisionRects()` and `renderOffscreenJumpTargets()`.
  - Added unit test `testHudActiveMarkerRect` in `tests/hudlayout_test.cpp`.

## [1.6.0-beta45] - 2026-10-31

### Fixed
- **Sequential HUD Badge Cycling with `Shift + ,` (`<`) and `Shift + .` (`>`)**:
  - Fixed an issue where cycling badges with `Shift + ,` or `Shift + .` in HUD jumped to the recent window and bounced between 2 windows instead of advancing sequentially through all badge targets.
  - Preserved origin active window reference (`m_hudOriginWindow`) so that activating windows during cycling does not dynamically reshape or shuffle `offscreenTargets` (dock pills) and on-screen badges in `paintScreen()`. Dock pills now remain completely stable and stationary while cycling.
  - Ensured `hudRefWindow` is included in dock pills when `BadgeHudIncludeAll` is enabled or window badges are disabled.
  - Re-implemented `slotCycleBadge()` to aggregate all badge targets (HUD dock pills, on-screen badges, window groups, and origin window) and sort them identically to the HUD dock (pinned/stashed on left, regular on right, ordered oldest to youngest from left to right).
  - `Shift + .` (`>`) advances to the next badge to the right (+1 towards younger windows), and `Shift + ,` (`<`) moves to the previous badge to the left (-1 towards older windows), wrapping around cleanly across all badges without looping or getting stuck.
  - Updated unit test `testHudShiftBadgeCycling` in `tests/superjump_test.cpp`.

## [1.6.0-beta44] - 2026-10-31

### Removed
- **Global `Shift + Meta + ,` and `Shift + Meta + .` Shortcuts**:
  - Removed default and global shortcut bindings for `PrevRecentWindow` and `NextRecentWindow` in favor of standard KDE Plasma `Alt + Tab` window switching.
  - Removed outside-HUD keyboard grab interception for history navigation from `grabbedKeyboardEvent`, leaving keyboard interaction untouched outside HUD mode.
  - Updated `install-leapk.sh` to migrate `PrevRecentWindow`, `NextRecentWindow`, `OpenWorkspaceManager`, and `OpenMarkerManager` to `"none"`.

### Changed
- **HUD Badges Recency Sorting (Oldest on Left, Youngest on Right)**:
  - In HUD mode, badges (`offscreenTargets`) are now sorted strictly by recency from right to left: the oldest (or unvisited) windows appear on the far left, while the youngest (most recently active) windows appear on the far right.
  - When pinned windows or stash mode partitions are present, both left and right groups maintain this oldest-to-youngest visual sorting within their sections.
  - Window history snapshot (`m_hudSnapshotHistory`) is captured upon opening HUD, ensuring badge pill positions remain stable and stationary while cycling.
  - `slotCycleBadge()` uses `m_hudTargetPillRects` visual screen order: `Shift + .` (`>`) cycles rightward towards younger/newer windows, and `Shift + ,` (`<`) cycles leftward towards older windows.
  - Added unit test `testHudRecencySorting` in `tests/superjump_test.cpp`.

## [1.6.0-beta43] - 2026-10-31

### Added
- **HUD Badge Cycling with `Shift + ,` (`<`) and `Shift + .` (`>`)**:
  - In all HUD modes (Standard Jump, Global Scope, Stash, SubLeap, Close, Pin), `Shift + ,` and `Shift + .` cycle focus through visible window badges according to preferred keyboard layout pool order.
  - Cycling smoothly activates the target window, warps cursor, updates outline highlight, unminimizes and switches desktop if needed, all while keeping the HUD open.
  - In Search Mode, `Shift + ,` / `Shift + .` cycle through matching search results.
  - Added `< / > Badges` segment to the HUD action hint bar in `badgehelper.h`.

### Changed
- **Immediate History Navigation Commit on Shortcut Release**:
  - In regular mode, `Shift + Meta + ,` and `Shift + Meta + .` now grab keyboard interaction during history navigation.
  - Releasing `Meta` or `Shift` immediately triggers `slotCommitHistoryNavigation()`, placing the chosen window at the top of history without any delay.
  - Reduced fallback commit timer to 600ms.
  - Added unit tests `testHistoryNavigationCommitOnRelease` in `tests/history_test.cpp` and `testHudShiftBadgeCycling` in `tests/superjump_test.cpp`.

## [1.6.0-beta42] - 2026-10-31

### Fixed
- **Window Disappearance After Alt+Key Pull into Workspace**:
  - Fixed a critical sequence bug where `cancelJumpMode()` was invoked prior to reading `m_superJumpActive` in HUD pull handlers (`SubLeap`, recent key match, group summon, and standard badge jump). Because `cancelJumpMode()` reset `m_superJumpActive` to `false`, workspace reassignment logic was bypassed as dead code, leaving the window unassigned to the target workspace.
  - Corrected `targetWs` evaluation across all pull handlers to properly prioritize `m_superJumpTargetWorkspace` when in Global Scope / Super Jump, or fallback to `m_activeHudView`.
  - Deferred `cancelJumpMode()` until after `addWindowToWorkspace()` and `applyWorkspaceViewIsolation()` execute.
  - Refactored `ActiveWindowOutlineEffect::addWindowToWorkspace` to use `LeapKWorkspace::reassignWindowToWorkspace()`, cleanly removing conflicting rules from other workspaces via bidirectional substring matching and updating `lastVirtualDesktopId` before saving.
  - Updated `removeWindowFromAllWorkspaces` in `workspacehelper.h` to match bidirectional substring semantics of `matchesWorkspace`.
  - Added comprehensive unit test `testSuperJumpPullIntoEmptyWorkspace` in `tests/superjump_test.cpp` verifying cross-workspace pull into empty workspaces, rule persistence, and isolation visibility retention.

## [1.6.0-beta41] - 2026-10-31

### Changed
- **Renamed "Super Jump" to "Global Scope"**:
  - Renamed the header banner in `badgehelper.h` from `SUPER JUMP` to `GLOBAL SCOPE`.
  - Updated the Meta-hold mode switcher cheat sheet overlay from `Super Jump` to `Global Scope`.
- **Cleaned Hint Syntax in HUD Action Bar**:
  - Removed vim-style parenthetical suffixes `(:bd)` from Close actions and `(:hide)` from Stash actions across all HUD sub-modes and default mode.
- **Removed "+ New • Shift++" Pill from HUD Workspace Header**:
  - Completely removed the `+ New • Shift++` button pill and its width/separator calculations from the HUD workspace header navigation bar.
- **Full Keyboard Navigation in Marker Manager**:
  - Added seamless `Esc` toggling: pressing `Esc` inside the search field exits search focus to navigate controls, while pressing `Esc` (or `/`) outside enters and selects the search field.
  - Assigned dedicated letter shortcuts to every control:
    - `[N]`: + New Rule
    - `[W]`: + Assign Window (when active window info is available)
    - `[X]`: Clear All Rules
    - `[Q]`: Close dialog
    - `[J]` / `[Down]`: Select next rule card
    - `[K]` / `[Up]`: Select previous rule card
    - `[E]` / `[Enter]`: Edit selected rule
    - `[D]` / `[Del]`: Delete selected rule
    - `[T]` / `[Space]`: Toggle active / disabled state of selected rule
    - `[1..9]`: Directly select visible rule card 1 through 9
    - `[A..Z]`: Direct jump letter selection matching rule letter
  - Added visual selection highlight and card index badges (`[1]`, `[2]`, ... `[9]`).
  - Added keyboard shortcut cheat strip directly in the Marker Manager dialog.
  - Added keyboard mnemonics and Alt-shortcuts to `EditMarkerRuleDialog` (`Alt+L`, `Alt+C`, `Alt+G`, `Alt+A`, `Alt+T`, `Alt+V`, `Alt+R`, `Alt+S`, `Alt+1..8`).
  - Added immediate persistence and live KWin D-Bus reload (`reconfigureEffect`) on every rule toggle, edit, creation, and deletion.

## [1.6.0-beta40] - 2026-10-31

### Changed
- **Removed Workspace Add/Delete Shortcuts from HUD**:
  - Removed `Shift++` (New workspace) and `_` (Delete workspace) from the bottom action hint bar.
  - Removed `isPlus` and `isUnderscore` / `Delete` key bindings from HUD keyboard routing in `grabbedKeyboardEvent`.
- **Added `Meta+W` to HUD Mode Switcher**:
  - Added `Meta+W` (Workspace Manager) to the Meta-hold cheat sheet overlay (`renderHudMetaOverlayImage`).
  - Pressing `Meta+W` inside HUD now cleanly closes the HUD and launches the Workspace Manager across all HUD modes (Jump, SubLeap, Stash, Close, Pin, Tiling).
- **Removed Global Shortcuts for Managers**:
  - Removed global shortcuts for `OpenWorkspaceManager` and `OpenMarkerManager` in `KGlobalAccel` and `kglobalshortcutsrc`. They are now cleanly launched directly via the HUD Meta switch mode (`Meta+W` and `Meta+M`).

## [1.6.0-beta39] - 2026-10-31

### Fixed
- **History Navigation Shortcuts (`Shift + Super + ,` / `Shift + Super + .`)**:
  - Resolved shortcut collision with KWin's built-in "Window to Next/Previous Screen" actions which previously claimed `Meta+<` and `Meta+>`.
  - Configured `PrevRecentWindow` and `NextRecentWindow` in KGlobalAccel to bind `Meta+<` and `Meta+>` (the physical keysyms emitted when pressing `Shift + Super + ,` and `Shift + Super + .`) alongside `Meta+Shift+,` and `Meta+Shift+.`.
  - Fixed keyboard event handling in `grabbedKeyboardEvent` to match `Qt::Key_Less` and `Qt::Key_Greater` generated by Shifted comma and period.
  - In `slotPrevRecentWindow` and `slotNextRecentWindow`, added automatic population from `stackingOrder` when history is initially short, guaranteeing instant window switching across all open windows.

## [1.6.0-beta38] - 2026-10-31

### Changed
- **Space in HUD Now Goes Back / Closes HUD**:
  - Pressing `Space` (or `Meta + Space`) while in any HUD sub-mode (SubLeap, Stash, Close Window, Pinning, Super Jump, Tiling Presets) exits the sub-mode back to the main HUD.
  - Pressing `Space` (or `Meta + Space`) while in the base HUD now cleanly dismisses and closes the HUD (`cancelJumpMode`).
  - Removed `Space` as the default jump-to-recent key match in HUD.
- **Global Window History Navigation Shortcuts (`Shift + Super + ,` / `Shift + Super + .`)**:
  - Rebound global actions for navigating window history:
    - Previous Recent Window: `Shift + Super + ,` (`Qt::META | Qt::SHIFT | Qt::Key_Comma`).
    - Next Recent Window: `Shift + Super + .` (`Qt::META | Qt::SHIFT | Qt::Key_Period`).
  - Preserved `Super + Space` exclusively for opening LeapK HUD (`JumpToWindow`).
  - Added seamless pass-through in `grabbedKeyboardEvent` to allow cycling window history with `Shift + Super + ,` and `Shift + Super + .` even while HUD is open.
- **HUD Hint Bar and Header Updates**:
  - Updated hint bars and header banners across all modes to show `␣ / ⌫ : Back` and `Esc / ␣ / ⌫ : Close`.

## [1.6.0-beta37] - 2026-10-31

### Changed
- **Close Window Mode Shortcut Updated to Meta+Q**:
  - Changed shortcut to enter, toggle, and exit Close Window mode from `Meta+X` to `Meta+Q`.
  - Updated key routing in HUD across standard jump mode, SubLeap mode, Stash mode, Pin mode, and Close Window mode.
  - Updated HUD top header banner, action hint bar, and Meta-hold cheat sheet overlay to reflect `Meta+Q`.

## [1.6.0-beta36] - 2026-10-31

### Changed
- **Declutter HUD Bottom Hint Bar**:
  - Replaced individual `Meta+A`, `Meta+X`, `Meta+P`, `Meta+S`, `Meta+U`, `Meta+T`, and `Meta+M` shortcut pills in the bottom action hint bar with a single clean indicator: `[Hold Meta] Switch mode`.
  - Added `Meta+M` (Marker Manager) to the modal cheatsheet overlay (`renderHudMetaOverlayImage`).

## [1.6.0-beta35] - 2026-10-31

### Added
- **HUD Meta-Hold Shortcut Cheat Sheet Overlay**:
  - Pressing and holding `Meta` (Super) while in HUD now renders a dedicated floating modal cheat sheet overlay in the center of the screen displaying all primary shortcuts and modal commands:
    - `Meta + A`: Super Jump (Scope toggle: All Workspaces + Stash).
    - `Meta + S`: Stash Mode (Drawer for stashing and un-stashing windows).
    - `Meta + X`: Close Window Mode (Batch close windows with red badges).
    - `Meta + U`: Audio Mode (Filter windows actively playing sound).
    - `Meta + P`: Pinning Mode (Pin or unpin windows across workspaces).
    - `Meta + T`: Tiling Presets (Choose tiling layout preset or slot edit).
  - Highlights active modes in real-time (`[ACTIVE]`), and smoothly disappears upon releasing `Meta` while persisting the selected mode.
- **Close Window Mode (`Meta + X`) with Batch Multi-Close**:
  - Dedicated window closing mode: all windows from the current workspace (or all workspaces + stash when Super Jump is toggled) appear in HUD and on-screen with vibrant red badges (`QColor(230, 57, 70)`) and close indicator (`✕`).
  - Pressing a window's jump letter immediately calls `target->closeWindow()`, closes the window, cleans up stashed/minimized states, and dynamically reassigns jump letters to remaining windows.
  - LeapK persists in Close Window mode after closing a window, enabling rapid multi-window batch closure without needing to re-open the HUD. Exits cleanly on `Esc`, `Enter`, `Backspace`, or `Meta + X`.
- **Universal Orthogonal Scope (`Meta + A`)**:
  - Transformed `Meta + A` into an orthogonal global scope modifier across all modes (Normal Jump, Close Window Mode, Pinning Mode, Stash Mode, etc.).
  - When `Meta + A` is active, queries and displays all windows globally regardless of where they reside (other virtual desktops, workspaces, and in stash), removing the previous stash exclusion.
  - In Close Window mode: pressing `Meta + A` exposes all windows across all workspaces + stash with red close badges for global batch closing.
- **Pinning Mode (`Meta + P`)**:
  - Interactive window pinning mode where pressing any window's jump letter toggles its pinned state (`pinWindow` / `unpinWindow`) across all workspaces.
  - Displays amber badges (`QColor(245, 166, 35)`) and pin indicators (`📌` / `+📌`), persisting in the mode until manual exit.
- **Extended Test Suite**:
  - Added `testCloseWindowModeBannerAndHintBar`, `testPinModeBannerAndHintBar`, and `testHudMetaOverlayRendering` in `tests/superjump_test.cpp`.

## [1.6.0-beta34] - 2026-10-31

### Fixed
- **Tiling Slot Window Isolation Across Workspaces**:
  - In `activateTilingPreset`, replaced broad desktop eligibility with strict workspace-aware qualification (`isEligibleForCurrentWorkspaceTiling`).
  - Prohibited pulling in windows from other workspaces via `m_windowHistory` (recents) or `effects->stackingOrder()`.
  - Windows minimized by workspace view isolation (`m_workspaceMinimizedWindows`) and user-minimized windows are now strictly excluded from automatic tiling population.
  - When a workspace contains fewer windows than slots in the preset (e.g. 1 window in a 2-slot or 3-slot layout), only the current workspace's windows are placed, leaving remaining slots empty without dragging in foreign windows.
  - In `performTilingSwap`, added empty slot detection: bringing in a background or unmanaged window now populates an empty slot first instead of displacing the active window.
  - In `slotWindowAdded`, newly launched windows on a tiled workspace automatically occupy the first available empty slot and resize to its geometry.
  - Added unit test `testTilingWorkspaceWindowIsolation` in `tests/tiling_test.cpp` to verify workspace window boundary isolation and empty slot handling.

## [1.6.0-beta33] - 2026-10-31

### Fixed
- **Synchronous Resizing Across Stacked and Grid Tile Slots**:
  - In `resizeSlotGeometries` (`tilinghelper.h`), replaced the single-neighbor lookup with a boundary divider algorithm that groups all aligned slots sharing the boundary.
  - When resizing a column divider (e.g. Master & Stack with 1 Master left, 2 Stack windows right), adjusting the Master slot now resizes BOTH stacked slots (Top and Bottom) in unison without leaving the bottom slot misaligned.
  - Symmetrically, resizing from any window in the stack adjusts the entire stack column against the Master slot, maintaining straight vertical column boundaries and identical gap spacing.
  - Added multi-slot divider support for 2x2 Quad Grid and multi-column layouts, moving shared row and column dividers synchronously across all intersecting slots.
  - Extended unit tests in `tests/tiling_test.cpp` (`testResizeSlotGeometries`) to verify multi-slot synchronization in Master & Stack and 2x2 Quad Grid across left/right/up/down resize directions.

## [1.6.0-beta32] - 2026-10-31

### Added
- **Tile Edit Mode (Direct Slot Swap & Vim-style Numeric Resize)**:
  - Added dedicated interactive Tile Edit Mode toggled via `Meta+Return` (Super+Enter) or `Meta+Enter`.
  - When activated on a tiled desktop, the active tile slot displays an `[EDIT]` or `[Nx]` multiplier badge, while all other tile slots display clean, single-letter swap target badges (`A`, `B`, `C`, etc. from `kTileEditKeys`).
  - **1-Key Instant Swap**: Pressing any displayed badge letter immediately swaps the active window with that tile slot, exchanging geometries, KDE quick tile management, and raising slots above background windows.
  - **Vim-Style Directional Resize**: `H` (Left), `L` (Right), `K` (Up), `J` (Down) and Arrow keys smoothly resize the divider between neighboring slots (base step: 5% of screen size).
  - **Numeric Multipliers**: Users can type numbers (e.g. `5h`, `3l`, `2j`) to multiply the step size (e.g. `5h` moves boundary by $5 \times 5\% = 25\%$ of screen width). Multiplier is displayed live on the active slot badge and bottom hint bar.
  - Added bottom floating hint bar: `[Letters: Swap Slot] • [1-9 + H/J/K/L: Resize (5% step)] • [Enter/Esc: Finish]`.
  - Added `resizeSlotGeometries` in `tilinghelper.h` and comprehensive test `testResizeSlotGeometries` in `tests/tiling_test.cpp`.

## [1.6.0-beta31] - 2026-10-31

### Fixed
- **HUD Closure When Switching Workspaces While Tiling Is Active**:
  - In `switchToWorkspaceView`, captured `wasInJumpMode` and passed `preserveJumpMode` to `activateTilingPreset(incomingPreset, wasInJumpMode)`.
  - In `deactivateTiling`, prevented calling `cancelJumpMode()` when transitioning between workspaces (`preserveWorkspaceState == true`).
  - When switching between workspaces (via HUD navigation tabs, key jumps, or window pills), HUD Mode is now preserved seamlessly with updated jump letters and full repaints.
  - Added unit test `testHudPreservationOnWorkspaceTilingSwitch` in `tests/tiling_test.cpp`.
- **Universal Window Rule for Minimum Size Limit**:
  - In `ensureTilingNoMinSizeRule`, explicitly written `wmclass=` and `wmclasscomplete=false` to ensure clean universal matching across all KWin 6 clients.

## [1.6.0-beta30] - 2026-10-30

### Fixed
- **Tile Slots Occlusion by Background Windows on Swap**:
  - In `performTilingSwap`, eliminated the erroneous `workspace()->stackAbove(slot, previousWin)` call which dragged remaining active tile slots down to the bottom of the window stack immediately above the evacuated window, allowing background windows to occlude them.
  - After lowering the evacuated window (`previousWin`) to the bottom of the stack, LeapK now explicitly raises all other active tile slots (`workspace()->raiseWindow(slot)`) and unminimizes them if necessary.
  - The newly swapped-in target window is then managed, resized, raised to the very top (`workspace()->raiseWindow(target)`), and focused (`effects->activateWindow(target)`).
  - Guarantees that all active tile slots remain strictly above all background floating windows in the window manager stacking order.
  - Added unit test `testTilingSwapStackingPreservation` in `tests/tiling_test.cpp` to verify that active tile slots maintain top stacking priority over background windows before and after tile swaps.

## [1.6.0-beta29] - 2026-10-30

### Fixed
- **Underlying Background Window Badges Occlusion by HUD**:
  - Implemented `computeAllHudCollisionRects` to comprehensively aggregate all HUD obstacle geometries: offscreen dock pill rects, full dock bounding rect (`allPillsBounds`), group container frames, group divider rects, HUD action hint bar (`computeHudHintBarRect`), and HUD workspace navigation header (`computeHudHeaderRect`).
  - Added `findBestBadgePositionForWindow` to evaluate on-screen jump badge placement against both overlapping foreground windows in KWin's stacking order and all HUD components.
  - When an underlying large window's default badge position is occluded by an active small window on top, it now systematically searches alternate positions (Top, Bottom, Corners, Sides) and rejects any candidate overlapping HUD Header, Hint Bar, or Dock.
  - If a background window's visible area is entirely covered by foreground windows or HUD elements, it safely demotes the window to an offscreen card in the HUD dock instead of drawing an obscured on-screen badge.
  - Added unit test `testBackgroundWindowBadgeOcclusionAvoidance` in `tests/badge_test.cpp` to verify collision avoidance for underlying windows across top and bottom HUD layouts.

## [1.6.0-beta28] - 2026-10-30

### Fixed
- **Tiled Windows Minimization on HUD Activation**:
  - Removed harmful `applyWorkspaceViewIsolation` call from `slotTriggerJumpMode()`: simply opening HUD/Jump Mode must never minimize windows on the user's active desktop.
  - In `applyWorkspaceViewIsolation`, actively tiled windows (`isWindowInTileSlot`) are now explicitly protected from minimization while tiling is active.
  - In `assignJumpLetters()`, all actively tiled windows on screen are guaranteed to receive jump badges so the user can easily jump across tiled columns.
  - In `findWorkspaceForWindow`, windows actively participating in a workspace's tiling layout are associated with the active workspace, preventing workspace switches when jumping between tile slots.

## [1.6.0-beta27] - 2026-10-30

### Fixed
- **Spontaneous Cross-Workspace Window Reassignment on Desktop Transition**:
  - Eliminated automatic rule reassignment and `effects->windowToDesktops` calls in `slotWindowActivated`: focusing a window must never steal its workspace rules or alter its virtual desktop assignment.
  - In `switchToWorkspaceView`, `applyWorkspaceViewIsolation` is now executed *before* switching KWin's active virtual desktop (`effects->setCurrentDesktop`). This ensures non-matching windows from other workspaces are minimized before arriving at the target desktop, preventing KWin from focusing them during desktop transition.
  - In `applyWorkspaceViewIsolation`, `topMatchingWindow` now prioritizes windows on the incoming target virtual desktop, preventing focus jumps across unrelated virtual desktops.
  - In `slotWindowAdded`, newly created windows are only added to the active workspace if they do not already match an existing workspace rule (`findWorkspaceForWindow == 0`).

## [1.6.0-beta26] - 2026-10-30

### Added
- **Per-Workspace Last Active Virtual Desktop Memory**:
  - LeapK now tracks and remembers the last active virtual desktop (`lastVirtualDesktopId`) per workspace when navigating away.
  - When switching back into a workspace (via HUD navigation tabs, key jump, window pills, or cycling), LeapK automatically transitions KWin to that workspace's last active virtual desktop before window isolation and slotting.
  - Serialized and persisted in `workspaces.json` and `Effect-leapk` config (`StandardLastVirtualDesktopId`).
- **Per-Workspace Tiling Layout and State Preservation**:
  - Tiling preset and active tiling state (`tilingPresetId`) are now scoped and preserved per workspace.
  - Navigating away from a tiled workspace safely unslots its windows from KDE quick tiles without discarding its active preset state.
  - Navigating back to the workspace automatically restores and reapplies its active tiling preset to the windows on that workspace's active virtual desktop.
  - Supported across Standard, Custom, and All views, and persisted across sessions.
- **Interactive HUD Workspace Header Tabs**:
  - Bounding rectangles for workspace navigation tabs in the HUD mode header are now tracked and mapped to screen space (`m_hudTabScreenRects`).
  - Users can click directly on any workspace tab in the HUD header to switch workspaces with sound feedback.

## [1.6.0-beta25] - 2026-10-30

### Fixed
- **Tiling Swap Window Stacking Order**:
  - Evacuated windows from tile slots during smart swap are now explicitly pushed to the bottom of the window stacking order using `workspace()->lowerWindow()` and actively restacked below all current tile slots with `workspace()->stackAbove()`.
  - Prevents evacuated windows from covering active tiled windows when their restored floating geometry overlaps the tiling grid.
  - When activating tiling presets, all tiled windows are now raised in reverse order so no floating background windows can sit between or obscure active tile slots.
- **Forced Minimum Window Size Removal for Tiling**:
  - Integrated automatic enforcement of a dedicated KWin window rule (`a9e701bc-6258-4903-a185-3b95a8bfb621`) configured with `minsize=1,1` (Force) and `ignoregeometry=true` (Force) for normal application windows.
  - Windows tiled into slots now invoke `Window::evaluateWindowRules()` prior to geometry resize, allowing applications with high minimum size constraints (such as browsers, messengers, and utilities) to shrink freely and conform to narrow tiling columns or split slots without overflowing.
  - Automatically verifies and synchronizes the rule in `~/.config/kwinrulesrc` on startup and preset activation, notifying KWin's `RuleBook` immediately.

## [1.6.0-beta24] - 2026-10-30

### Fixed
- **KDE Custom Tiling Layout Integration & Multiplication Fix**:
  - Fixed a critical bug where repeatedly switching or activating tiling presets caused `RootTile` children to accumulate without being cleared, multiplying column slices in `kwinrc` (`[Tiling][deskId][outUuid]`) and cluttering KDE Custom Tiling with unexpected narrow columns.
  - Prior to applying external presets, `rootTile->destroyChild()` is now safely called on existing child tiles so new preset layouts replace the existing tree rather than appending to it.
  - Eliminated duplicate reversed `[Tiling][outUuid][deskId]` keys in `kwinrc`, preserving the canonical KDE `[Tiling][deskId][outUuid]` format.

### Added
- **Dynamic "KDE Custom Layout" (`[C]`) Integration**:
  - Presets mode now dynamically queries KWin's live `RootTile` and inspects the user's custom tile arrangement configured in KDE.
  - Introduces the **"KDE Custom Layout"** preset (key `C`, or `Enter` by default), rendering the exact slots and dimensions configured by the user in KDE Custom Tiling (e.g. 2 columns, custom split ratios).
  - Selecting `[C]` slots windows directly into the user's custom KDE tiles without modifying or overwriting `kwinrc`.
- **Direct KDE Tile Editor Invocation (`E`)**:
  - Pressing `E` while in LeapK Presets mode dismisses the Jump HUD and immediately opens KDE's native Tile Editor (`Edit Tiles`) via DBus.
  - Updated HUD action hint bar and header text to display `[C] KDE Custom` and `[E] Edit in KDE`.

## [1.6.0-beta15] - 2026-10-30

### Added
- **KDE 6 Built-in Custom Quick Tile Synchronization**:
  - Overwrites and keeps in sync the native KDE Custom Tiling layout in `kwinrc` (`[Tiling][deskId][outUuid]` and `[Tiling][outUuid][deskId]`) and live `TileManager` / `RootTile` hierarchy.
  - Tiles windows directly into KWin's native `Tile` nodes using `Tile::manage(Window*)` / `unmanage(Window*)`, ensuring full compatibility with KDE's built-in Tile Editor (`Meta+T` / Tile Mode).
- **Exact Pre-Tiling Geometry Restoration**:
  - Remembers original window positions and dimensions before tiling in `m_preTilingGeometries`.
  - Fully restores all window geometries when exiting tiling mode (via `X`, float preset, or dragging a window titlebar).
  - When swapping in an external window, the replaced occupant's pre-tiling geometry is cleanly restored upon yielding the tile slot.
- **Strict Current-Desktop Window Gathering**:
  - Tiling preset window allocation strictly filters to windows belonging to the current virtual desktop (`w->isOnDesktop(currentDesktop)`).
  - Prioritizes: 1. Active window on current desktop, 2. MRU history on current desktop, 3. Remaining windows on current desktop from `stackingOrder` (top-to-bottom). Never pulls windows across virtual desktops.
- **Tiling Navigation vs. Smart External Swap**:
  - Selecting a window that is already in a tile slot directly focuses and warps the cursor to that window without modifying the tile layout.
  - Selecting a window outside of the tiling grid (minimized, background, stash) performs an atomic swap into the active tile slot.
- **Shortcut Update to `Meta+T`**:
  - Presets mode can now be toggled directly using `Meta+T` both globally (via KGlobalAccel) and from within the Jump HUD.

## [1.6.0-beta14] - 2026-10-30

### Added
- **Integrated Tiling System (Tiling Presets & Smart Swap)**:
  - **Independent Tiling Presets**: User-definable layouts independent of virtual desktops or workspaces (`tilinghelper.h`, `~/.config/leapk/tiling_presets.json`).
  - **Preset Selector in HUD (`Shift+T`)**: Invoked directly from LeapK Jump HUD (`Meta+Space` $\rightarrow$ `Shift+T`), presenting visual preset cards (`[A]` Two Columns `▌▌`, `[S]` Three Columns `▌▌▌`, `[D]` Master & Stack `▌☰`, `[F]` 2x2 Quad Grid `⊞`, `[G]` Full Focus `🗖`, `[X]` Float / Turn off).
  - **Auto-populate with MRU Windows**: Activating a preset instantly slots in the top $N$ most recent windows from `m_windowHistory` with configurable screen gaps and outer margins.
  - **Smart SWAP via HUD Selection**: When tiling mode is active, picking any window from HUD (`Meta+Space` $\rightarrow$ key or click) executes an atomic swap:
    - If the target is already in another tile slot on screen, the two windows swap slots seamlessly.
    - If the target is incoming from background / minimized / stash, it adopts the active tile slot geometry and the previous occupant yields to the background.
  - **Natural Mouse Drag Auto-Disengagement**: Dragging a window by its titlebar (`isUserMove()`) immediately disengages tiling mode and returns windows to floating. Dragging shared tile borders (`isUserResize()`) preserves tiling mode while updating slot geometries.
  - **Automated Test Suite**: Added `outline-tiling-test` (`tests/tiling_test.cpp`) covering geometry math, relative-to-absolute calculations, padding/margins, and preset JSON serialization.

## [1.6.0-beta13] - 2026-10-29

### Fixed
- **Group Disambiguation (Sub-Leap) Letter & Card Display Bug**:
  - Resolved critical issue where pressing a group letter key (e.g. `T` for 3 terminal instances) entered "group disambiguation" mode but displayed zero HUD cards or selection letters.
  - Corrected inverted `!forceHud` condition in `m_subLeapActive` target classification where `BadgeHudIncludeAll=true` mistakenly caused obscured, minimized, or covered group instances to be dropped.
  - Implemented unified, robust candidate classification (`OutlineBadge::classifySubLeapTarget`) guaranteeing all disambiguation candidates are displayed in the HUD dock (when `hudIncludeAll` or `hudGroupWindows` is active), while visible unobscured windows on the current desktop also receive on-screen badges.
  - Suppressed redundant group instance counter `(N)` on individual candidate cards during group disambiguation.
  - Added comprehensive automated test suite `testSubLeapTargetDistribution` to verify 100% inclusion of all candidate windows in disambiguation mode.

## [1.6.0-beta12] - 2026-10-29

### Fixed
- **Persistent Window Markers Not Saving & Window Reopen Restoration**:
  - Resolved critical issue where persistent mark creation was mistakenly nested inside the letter collision `reassign` check in `slotMarkWindowWithColor()`. Persistent marks now reliably write to `~/.config/leapk/marks.json` on all assignments.
  - Added `m_marksWatcher` (`QFileSystemWatcher`) and D-Bus synchronization so external edits in Marker Manager or CLI immediately reload and reflect in running KWin instances without requiring logout.
  - Added delayed re-check and initial frame geometry change detection in `applyPersistentMarksToWindow()` to handle Wayland compositors that populate `app_id` asynchronously after surface mapping.
  - Fixed duplicate rule generation when re-assigning or changing letters for an existing application mark rule.

### Changed
- **Streamlined Settings UI for Persistent Marks**:
  - Removed redundant embedded table and rule management buttons (`tablePersistentMarks`, delete, clear all, reload) from LeapK Settings (`tabBehavior`).
  - Added dedicated launcher card with real-time rule count and a button to open the full standalone LeapK Marker & Group Manager (`leapk-marker-manager`).
  - Added `leapk-marker-manager.desktop` launcher for KDE Application Launcher and KRunner integration.

## [1.6.0-beta5] - 2026-10-29

### Added
- **Declarative Visual Theme Engine (`themehelper.h`, `.leapk-theme`)**:
  - Complete architectural separation of **Presets** (behavior/shortcuts), **Shaders** (GLSL perimeter effects), and **Themes** (complete visual aesthetic).
  - Component-level declarative styling for Window Badges, HUD Dock Containers, Window Cards, Search Header, and Action Hints Bar.
  - Supports versatile geometric shapes: `pill`, `rounded_rect`, `sharp`, `chamfered` (45° cut sci-fi corners), and `circle`.
  - Four built-in visual themes:
    - **Breeze Dark (Default)**: Modern, balanced KDE Plasma 6 aesthetic with rounded pill badges.
    - **Cyberpunk Neon**: Sci-fi HUD with chamfered cut corners, cyan/magenta neon glow, and terminal prompt `❯`.
    - **Retro Matrix Terminal**: Sharp phosphor green CRT aesthetic with bracketed keycaps `[A]` and wireframe cards.
    - **Nordic Frost Glass**: Minimalist frosted glass aesthetic with fluid pill shapes, soft shadows, and pastel palette.
  - Variable token resolution allowing theme components to reference dynamic parameters (e.g. `$accentColor`, `$neonCyan`, `$phosphorColor`).
  - Import and Export of standalone `.leapk-theme` and JSON packages to `~/.local/share/kwin-active-window-outline/themes/`.
- **Dynamic Theme Customizer & Tweaks Tab in Settings (`tabThemes`)**:
  - Dedicated Themes management tab with theme status table, double-click activation, import, and export.
  - Automatically generates native Qt configuration widgets (`KColorButton`, `QSpinBox`, `QDoubleSpinBox`, `QCheckBox`, `QComboBox`) for any parameters exposed by the theme author (`ThemeParameter`).
  - Keeps the main settings panel clean and uncluttered while giving theme authors unlimited customization power.
- **HUD Action Hints Bar Toggle Shortcut (`?`)**:
  - Pressing `?` while HUD jump mode is active toggles the bottom action hints bar on or off in real time.
  - Automatically persists the user's preference to `kwinrc` (`BadgeHudShowHints`).
  - Added dedicated hint segment `[?] Hints` to the action bar.
  - Configurable in LeapK Settings (`kcfg_BadgeHudShowHints`).
- **Independent HUD Font Size Controls (`BadgeHudNavFontSize`, `BadgeHudHintsFontSize`)**:
  - Replaced ambiguous HUD size slider with dedicated typography controls:
    - **HUD Navigation Font Size**: scales workspace tabs, query titles, scope pills, and badge counters.
    - **HUD Hints Font Size**: scales action hint keycaps and description labels.
  - Both settings support auto-scaling (`0` / `Auto`) or custom pixel sizing.
- **Dynamic WCAG Contrast for Audio Speaker Indicator**:
  - Vector speaker icon dynamically calculates WCAG luminance contrast against the underlying card/tab/badge background.
  - On light, bright, or yellow backgrounds (where standard yellow fails contrast), automatically adapts to a high-contrast dark amber or deep contrast color.
  - Added a subtle backing halo ensuring razor-sharp icon visibility on any wallpaper or background surface.

## [1.6.0-beta4] - 2026-10-28

### Added
- **Configurable Cursor Warp After Window Jump (`CursorWarpMode`)**:
  - Automatically teleports the mouse cursor to the target window upon landing.
  - Three configurable modes: `0` (Disabled), `1` (Center of Window), and `2` (Top-Left Titlebar).
  - Works across all 9 jump pathways: direct window leap, sub-leap disambiguation, recent window jump, Super Jump, Stash pull, Pin & Drop, and Fuzzy Search.
  - Full configuration synchronization across settings GUI (`leapk_config.ui`), KConfig schema (`leapkconfig.kcfg`), shell scripts, and presets.
- **Audio Playing Indicator & Direct Audio Jump Mode (`Meta+Ctrl+A` / `Meta+U`)**:
  - Module `audiotracker.h` detects applications actively playing audio using PipeWire and PulseAudio sink inputs (`pactl` stream inspection) and MPRIS D-Bus state (`org.mpris.MediaPlayer2.*`).
  - Deep ancestor process resolution (`/proc/<pid>/stat`) maps background audio streams to top-level X11/Wayland windows across sandboxed apps (Flatpak, Electron, Spotify, Chrome).
  - Dynamic vector speaker icon with sound waves drawn directly on HUD tabs, workspace pills, and window badges.
  - Dedicated global shortcut `JumpToAudioWindow` (`Meta+Ctrl+A`) and in-HUD interactive filter toggle (`Meta+U`) to instantly isolate and leap to audio-producing windows.
- **Interactive Fuzzy Window Search with Scope Filters (`/`, `Ctrl+F`, `Meta+Ctrl+F`, `Meta+/`)**:
  - Subsequence matching algorithm (`fuzzymatcher.h`) with word-boundary, CamelCase, consecutive match, and acronym scoring bonuses.
  - Floating HUD Search Bar featuring query input, blinking cursor, match counter, and scope pills:
    - `[💻 Current WS]` (Current Virtual Desktop)
    - `[🌐 All]` (All Workspaces)
    - `[📦 All + Stash]` (All Workspaces + Hidden Stash Drawer)
  - Interactive scope switching via `Tab` / `Shift+Tab`.
  - Live candidate selection via `↑` / `↓` or `Ctrl+P` / `Ctrl+N` with glowing cyan highlight border.
  - Press `Enter` to jump, `Alt+<Key>` for direct badge leap, or `Backspace` / `Esc` to clear and exit.
  - Global shortcut `JumpToSearchWindow` (`Meta+Ctrl+F` and `Meta+/`).
- **Subtle Tactile Audio Feedback (`SoundFeedbackEnabled`, `SoundFeedbackVolume`)**:
  - Zero-dependency in-memory PCM WAV generator (`soundhelper.h`) synthesizing two tactile micro-sounds:
    - `EnterJump`: 12ms crisp tactile mechanical switch click (2200Hz base + 4600Hz transient).
    - `LandWindow`: 14ms soft damped landing snap (900Hz base + 2200Hz transient).
  - Ultra-low latency asynchronous playback via PipeWire (`pw-play`), PulseAudio (`paplay`), or ALSA (`aplay`).
  - Fully configurable in LeapK Settings (default: disabled, volume 0-100%).
- **Window Stash Drawer, Pin & Drop Across Workspaces, and Super Jump**:
  - High-contrast dual-ring pinned indicator dot and quick window pinning with `Alt+Key` / `Meta+P`.
  - Stash drawer to park secondary windows away from the active workspace.
  - Super Jump (`Meta+Space Meta+A`) to pull or switch to windows across all virtual desktops.

## [1.6.0] - 2026-10-27

### Added
- **Persistent Window Marks (`~/.config/leapk/marks.json`)**:
  - Window marks and custom letters can now be permanently remembered across desktop sessions and system restarts.
  - Multi-attribute window matching algorithm scoring application ID (`windowClass`), window caption patterns, and virtual desktop index.
  - Integrated "Remember permanently for this application" toggle in `leapk-colorpicker` GUI dialog.
  - Automatic mark restoration upon window creation and effect reconfiguration.
- **Dynamic Keyboard Layout Awareness & Home-Row Prioritization**:
  - Automatic detection of active keyboard layouts from KDE Plasma (`kxkbrc`), `$XKB_DEFAULT_LAYOUT`, and system locale.
  - Native support for QWERTY, QWERTZ, AZERTY, Colemak, and Dvorak layouts.
  - Letter assignment algorithm dynamically prioritizes ergonomic home-row keys first (e.g. `ASDFGHJKL` on QWERTY, `ARSTDHNEIO` on Colemak, `AOEUIDHTNS` on Dvorak) before reaching to top or bottom rows.
- **Sub-Leap Disambiguation & Spatial Neighbor Keys**:
  - When multiple windows share the same mark letter (e.g. `F` for Firefox across 2 or more open windows), selecting that letter enters an interactive Sub-Leap disambiguation mode.
  - Disambiguation instances are presented in the screen edge HUD dock and with on-screen badges using physical spatial neighbor keys.
  - Centered 3-key layout (`[ D | F | G ]`) ensures double-tapping the base key (`F + F`) immediately activates the primary/most recent window instance.
  - Physical adjacent neighbor keys immediately leap to the corresponding sibling instances.
  - Pressing `Escape` or `Backspace` smoothly returns to the main Jump Mode without dismissing navigation.
- **Unified Codebase & Clean Rebranding to LeapK**:
  - Complete transition of source files, headers, configuration UI, schema, and binaries to `leapk` (`leapk.cpp`, `leapk.h`, `leapk_config.cpp`, `leapkconfig.kcfg`, `kwin_leapk_config.so`).
  - Seamless migration of user settings from `[Effect-active-window-outline]` to `[Effect-leapk]` in `kwinrc`.
  - Comprehensive migration and cleanup in `install-leapk.sh` and `uninstall-leapk.sh`, automatically cleaning up legacy shared libraries, desktop launchers, autostart entries, and config entries.
  - Dedicated unit tests for keyboard layout algorithms (`outline-keyboardlayout`) and persistent mark serialization/scoring (`outline-persistentmark`).

## [1.6.0-beta2] - 2026-10-26

### Added
- **Live Window Previews Option (`BadgeLivePreviews`)**:
  - Added an option to continuously refresh window thumbnails in real time (up to 30–60 fps) while badges or edge HUD cards are visible.
  - Enables smooth real-time monitoring of playing videos, browser animations, and terminal output inside badges without UI lag or GPU stalls.
  - Full configuration synchronization across settings GUI, KConfig schema, shell installers, uninstallers, and D-Bus IPC.
- **HUD Include All Windows Option (`BadgeHudIncludeAll`)**:
  - Added an option to display cards in the edge HUD for all open windows, regardless of whether they are visible on screen, covered, minimized, or on other virtual desktops.
- **Badges Display Timeout (`BadgeTimeout`)**:
  - Added configurable timeout in seconds (0–300 s, default 5 s) before badges auto-hide during Jump Mode (0 = unlimited / no timeout).
- **Option to Disable On-Window Badges (`BadgeWindowBadgesEnabled`)**:
  - Added an option allowing users to toggle off on-window badges completely (`BadgeWindowBadgesEnabled = false`), keeping documents, terminals, and web browsers completely clean without letter overlays.
  - When disabled, all navigation targets are presented exclusively through the screen edge HUD dock bar.
  - Full configuration synchronization across settings GUI, KConfig schema, shell installers, uninstallers, and D-Bus IPC.
- **16 Thoughtfully Designed Built-In Presets for LeapK v1.6 with Custom GLSL Shaders & Enlarged Previews**:
  - Overhauled and expanded built-in presets to 16 distinct profiles with deep parameter tuning, embedded GLSL fragment shaders, and significantly enlarged HUD preview cards:
    1. *LeapK Default (v1.6)*: Balanced daily driver with 2px KDE accent outline, top jump badges, 180×100 px bottom HUD preview cards, and Space jump.
    2. *Vim Motion & HUD Center*: Command center for Vim/i3 tiling power users: background letter badges, 160×90 px HUD cards, multi-level focus trail, zero latency.
    3. *Live Vision Dashboard*: Multitasking powerhouse with continuous real-time live window thumbnails (30–60 fps), massive 320×180 px HUD cards (56 px HUD dock), 160×90 px window badges, and ambient glow.
    4. *Electric Plasma Storm*: **Custom Shader: `electric_plasma`** (high-voltage energy arcs, voltage 1.8, spark frequency 12, glow boost 1.5, arc color `#00d4ff`), 220×124 px HUD cards.
    5. *Matrix Digital Rain*: **Custom Shader: `matrix_rain`** (green digital code rain `#00ff66`, speed 1.6, density 24, trail length 1.2, sharp 0px corners, left-docked 180×100 px HUD).
    6. *Inferno Blaze & Embers*: **Custom Shader: `fire_aura`** (animated blazing flames and rising embers, speed 1.4, turbulence 3.2, flame `#ff3300`, core `#ffea00`), 240×135 px HUD cards.
    7. *Cyberpunk Hologram 2077*: **Custom Shader: `futuristic_glitch`** (holographic scanlines, chromatic anomaly sweep, neon flicker), 220×124 px top HUD cards.
    8. *Ambilight Screen Cinema*: Cinema reactive edge lighting with real-time screen edge sampling, 16:9 cinema wide 260×146 px HUD cards, 120×68 px window badges.
    9. *Nordic Frost & Aura*: Calming Scandinavian Nord aesthetic with 4px floating offset border, arctic ice cyan (`#88c0d0`), breathing pulse, soft aura, and 200×112 px HUD cards.
    10. *macOS Sonoma Minimal*: Clean, understated modern minimalism: ultra-thin 1px crisp outline, generous 10px rounded corners, no glow or delay, compact pill badges, zero previews.
    11. *Tiling Matrix Trail*: Strict tiling geometry: sharp emerald (`#00ff66`) with zero corner radius, 4-step focus breadcrumb trail, and left-docked vertical 180×100 px HUD cards.
    12. *RGB Chroma Spectrum*: **Custom Shader: `rainbow_wave`** (flowing prismatic rainbow spectrum hues), 200×112 px HUD cards.
    13. *High-Contrast Amber A11y*: Maximum visibility and cognitive clarity: bold 5px safety amber border (`#f39c12`), full 100% opacity, enlarged typography, persistent badges, 220×124 px HUD cards.
    14. *Retro CRT Amber Phosphor*: Vintage 1980s mainframe terminal: warm amber phosphor (`#ffb000`), subtle CRT flicker, gentle breathing, square corners, contrast-protected inverse video, and 200×112 px top-docked HUD.
    15. *Focus Zen Distraction-Free*: Deep focus and writing: discreet 1px graphite outline with gentle 35% opacity, zero animations or glow, jump badges strictly on demand with fast 3-second auto-hide.
    16. *HUD Dock Navigator*: Focuses navigation entirely on a bottom edge HUD dock for all open windows: on-window badges are hidden for a clean workspace, featuring large 240×135 px 16:9 previews, app icons, and single-key jump.
  - Built-in presets seamlessly bundle custom GLSL shaders via `makeShaderBundle()`, automatically registering shader code and uniform parameters upon preset activation.
  - Fully synchronized all 127 configuration keys across `getDefaultSettings()`, presets catalog, and `PresetActivationDialog` categories.
- **Jump to Recent Window Shortcut in LeapK Settings (`JumpRecentKey`)**:
  - Migrated "Jump to recent window" from KDE global shortcuts (`KGlobalAccel`) into LeapK settings as an exclusive modal 2nd-level jump key (default: `Space`).
- **Live On-Screen Badges Preview**:
  - Added live preview toggles for on-window badges and edge HUD cards with real-time updates directly from the settings module.

### Fixed
- **Window and HUD Badge Typography Contrast Safeguard**:
  - Fixed an issue where custom jump letter colors (such as amber `#ffb000` in the *Retro CRT Amber Phosphor* preset) blended invisibly with matching badge backgrounds (e.g. amber recent window or marked window badges).
  - Added `contrastRatio()` calculation based on perceived luminance in `renderBadgeImage()` and `renderOffscreenTargetPill()`. When the configured letter color does not meet the minimum contrast threshold (ratio < 2.5:1), the renderer automatically falls back to contrasting black or white text, ensuring jump letters and shortcut keys ("Space") remain razor-sharp and fully readable under all theme combinations.
  - Enhanced *Retro CRT Amber Phosphor* typography colors (amber labels, captions, and locations) with themed amber border outlines on dark backgrounds.
- **Window Badge Content Cut-Off with Recent Key Indicator (`recentKey` / "Space")**:
  - Fixed an issue where window badges on previously active windows had their content clipped or pushed offscreen when the recent shortcut indicator ("Space") was appended.
  - Added `recentSectionW` calculation and reservation into `effectiveBaseWidth` in `renderBadgeImage()`, ensuring the badge image buffer dimensions properly expand to fit the letter, shortcut indicator, icon, thumbnail, and custom label with uniform separator lines and margins.
- **Real-Time Live Previews Refresh & Obscured Window Cache Bypass**:
  - Fixed an aggressive caching trap in `captureWindowThumbnail()` that prematurely returned stale static frames for obscured, minimized, or background windows instead of executing offscreen rendering (Priority 3).
  - Connected `EffectWindow::windowDamaged` signal to invalidate thumbnail and badge texture caches when window contents change (e.g. playing videos, browser animation, terminal output).
  - Ensured `setBadgePreview()` clears `m_thumbnailCache` and `m_lastCaptureTimestamp` so any slider or checkbox adjustment in the settings dialog immediately regenerates and re-renders live previews.
- **Inverted Window Previews (Firefox DMABUF & OpenGL FBO Readbacks)**:
  - Fixed upside-down window thumbnail previews for hardware-accelerated Wayland clients using DMABUF (e.g. Firefox, Chromium, mpv) where OpenGL texture coordinate space inverted raw buffer memory.
  - Ensured all offscreen framebuffer (FBO) thumbnail readback paths (`blitFromRenderTarget` and `drawWindow`) are vertically flipped to match `QImage` top-left raster coordinates.

### Changed & Improved
- **HUD Occlusion Avoidance for Window Badges**:
  - Window badges are now treated as occluded when edge HUD cards overlap or cover them.
  - Automatically moves window badges to unoccluded alternative positions (Center, Top, etc.) when the configured position collides with HUD cards.
  - If no unoccluded position is available on the window, the badge is gracefully omitted from the window and displayed inside the HUD, preventing overlapping or clipped elements both in modal Jump Mode and during settings live previews.
- **Clean Jump-to-Recent Badge Layout (`[ Key | Space ]`)**:
  - Relocated the recent key indicator into the badge key capsule (e.g. `[ D | Space ]`), eliminating pollution of window custom labels and preserving original desktop locations and window titles.
- **Previously Active Windows Highlighting in HUD Cards**:
  - Previously active windows now receive distinct recent styling in HUD hint cards (vibrant border, recent accent color, and `· Ostatnie` / `· Recent` tag).
- **Expanded Limits for High-Resolution / 8K Displays**:
  - Significantly increased configuration maximums across outline and badge parameters:
    - Border width up to 100 px, outline gap up to 200 px, border radius up to 200 px, glow radius up to 200 px.
    - Window & HUD badge sizes and icon sizes up to 500 px.
    - Window thumbnails up to 2500 px, HUD thumbnails up to 3500 px.
    - Typography font sizes up to 200 px and corner radii up to 200 px.

## [1.6.0-beta1] - 2026-10-25

### Added
- **Modular Custom Shader Effects Architecture (GLSL)**:
  - Added full support for creating, editing, importing, and exporting custom GLSL fragment shader effects (`.leapk-shader` / `.json`).
  - **Dynamic Uniform Parameters**: Shaders can define arbitrary custom uniform variables of type `float`, `int`, `bool`, and `color` with custom labels, ranges, steps, suffixes, and default values.
  - **Zero Compilation Friction (JIT)**: GLSL fragment shaders are compiled on the fly at runtime by KWin's OpenGL pipeline — no C++ rebuilds, ahead-of-time SPIR-V compiling, or compiler toolchains required.
  - **Interactive Shader Editor Dialog (`ShaderEditorDialog`)**:
    - Manage effect metadata: Name, Description, Author, Version, and Target (Aura/Glow, Outline/Border, Both).
    - Built-in parameter designer: add, edit, or remove uniform parameters with live validation.
    - Full-featured monospace GLSL code editor with documentation hint banner for available inputs (`baseColor`, `pos`, `box`, `dist`, `time`).
    - Built-in GLSL syntax validator and starter template generator.
  - **Settings GUI Integration (Tab 3: Shader Effects)**:
    - Dedicated management section with action bar: *New Effect…*, *Edit…*, *Import…*, *Export…*, *Delete*.
    - Table of installed shader effects with right-click context menu.
    - *Active Effect Parameters*: dynamically generates interactive controls (sliders, spinboxes, checkboxes, color buttons) for the active shader effect.
  - **Companion Tray Menu Integration**:
    - Added *✨ Efekty Shaderów (GLSL)* submenu in tray context menu allowing one-click custom shader toggling and switching between installed profiles.
  - **4 Built-in Starter Shader Effects**:
    - *Rainbow Wave*: Smooth dynamic prismatic rainbow hue cycle flowing around the window perimeter.
    - *Matrix Digital Rain*: Cascading green code stream drops with tail decay and customizable matrix color.
    - *Electric Plasma*: High-voltage electric arcs and energy crackle pulsating along the outline.
    - *Fire Aura*: Warm blazing flames and rising embers radiating from the window perimeter.
  - **Comprehensive Regression & Synchronization Tests**:
    - Added `outline-customshader-test` verifying effect serialization, deserialization, GLSL uniform stitching, code validation, and file persistence (15/15 unit tests passing).
- **Selective Preset Activation Dialog (`PresetActivationDialog`)**:
  - Added an interactive customization dialog when activating any preset from the Settings GUI or Companion Tray app.
  - Allows selecting exactly which categories or individual configuration settings to apply from the preset, while preserving existing configuration for unselected options.
  - Full hierarchical grouping covering all 76 settings:
    1. Geometry & Border (`BorderWidth`, `OutlineGap`, `BorderRadius`, `FadeCorners`, `DrawInside`, `SmoothEdges`, `GlowEnabled`, `GlowRadius`)
    2. Colors & Opacity (`UseAccentColor`, `CustomColor`, `AccentOpacity`, `EnsurePerimeterContrast`)
    3. Dynamic Ambilight (`AmbilightEnabled`, `AmbilightMode`, `AmbilightOpacity`, `AmbilightFrequency`, `AmbilightDuration`, etc.)
    4. Shader Effects & Animations (`Breathe`, `Glitch`, `NeonFlicker`, `Flash`, `ColorCycle`)
    5. Leap Mode & Badges (`BadgeDisplayMode`, `JumpTargets`, `BadgePosition`, `BadgeHudPosition`, `BadgeSize`, `BadgeMargin`)
    6. Window Marks (`MarkDefaultColor`, `MarkSmartAdaptive`, `MarkRandomColor`, `StylePrecedence`, `MarkInactive...`)
    7. Focus History & Behavior (`HighlightRecentWindow`, `RecentWindowsCount`, `FadeDuration`, buffer delays, etc.)
  - Real-time side-by-side comparison: displays preset values alongside your current values, highlighting changes in green.
  - Quick selection toolbar:
    - *Select All* (☑ Zaznacz wszystko)
    - *Deselect All* (☐ Odznacz wszystko)
    - *Differences Only* (⚡ Tylko różnice) – automatically selects only the settings that differ from your active setup.
  - Real-time search filter box for instant lookup across all categories.
  - Dual activation actions: one-click *Apply All* (⚡ Zastosuj całość) or *Apply Selected* (✓ Zastosuj zaznaczone).
  - Added *Customize & Apply Preset…* (⚙️ Dostosuj i aktywuj preset…) submenu in the companion tray menu.
- **Preset Editing & Management Cleanup**:
  - Added dedicated *Edit…* (Edytuj…) dialog allowing editing preset name and description directly.
  - Added right-click context menu on the presets table with shortcuts to activate, edit, duplicate, delete, and export.
  - Removed redundant global bottom "Save Preset" button and streamlined the Presets tab action bar to "Save Current as Preset…" (Zapisz obecne jako preset…).
  - Creating a new preset from current settings now prompts for both Name and Description.

### Fixed
- **Preset System & Vim HUD Navigator**:
  - Fixed `Vim HUD Navigator` preset `BadgeDisplayMode` from `2` (Disabled) to `1` (Always visible on background windows), resolving an issue where Jump Mode was disarmed and the HUD could not display.
  - Ensured preset import never auto-activates unexpectedly.

## [1.5.2] - 2026-10-25

### Fixed
- **Companion Tray Update Dialog & "Open Release Page" Link Fix**:
  - Unified the update dialog across the companion tray app and settings GUI (`updatehelper.h`).
  - Fixed an issue where clicking "Open Release Page" failed to open the browser or prematurely dismissed the dialog without launching the page.
  - Implemented robust browser launching with fallbacks (`QDesktopServices::openUrl`, `xdg-open`, and `gio open`).
  - The tray update check now displays the complete update dialog identical to Settings, including release notes, one-click terminal install/update command, "Copy Command" button, and release page launcher.
  - Added an interactive up-to-date confirmation dialog with a direct button to view the latest release notes on GitHub.

### Changed
- **Refined Jump Targets Scope Options (`JumpTargets`)**:
  - Clarified that cross-desktop leaping covers both visible and minimized windows.
  - Split jump targets into 4 clean, dedicated scopes:
    1. Current desktop visible windows + marked windows from all desktops (default).
    2. Current desktop all windows (visible & minimized) + marked windows from all desktops.
    3. All windows across all virtual desktops (visible & minimized).
    4. Marked windows only (all desktops).
  - Added localized Polish descriptions in settings for all 4 scopes.

## [1.5.1] - 2026-10-25

### Fixed
- **Multi-Monitor Desktop Surface Outline Bug**:
  - Fixed an issue on multi-monitor configurations where clicking or focusing the desktop on one screen caused desktop surfaces (or wallpaper layers) on secondary screens to be outlined or marked.
  - Implemented `isEligibleOutlineWindow()` to strictly filter out desktop surfaces (`isDesktop()`), docks/panels (`isDock()`), special/popup/tooltip windows, and notifications.
  - Added guards across window activation, fade-out tracking, delayed hiding, and window mark toggling to prevent desktop surfaces from ever receiving outlines or polluting the window focus history.
- **KWin < 6.6 Build Compatibility**:
  - Resolved build failure on KWin versions prior to 6.6 (`error: 'Rect' does not name a type; did you mean 'QRect'`).
  - Introduced version-aware `OutlineRect` type alias (`QRect` on KWin < 6.6, `KWin::Rect` on KWin >= 6.6) for OpenGL ambilight color sampling and blit operations.

### Changed
- **Documentation & Cleanup**:
  - Streamlined `README.md` with an upfront `TL;DR` section highlighting features and 1-line installation.
  - Enhanced installer and uninstaller scripts to ensure complete cleanup of legacy desktop entries and stale launcher references.

## [1.5.0] - 2026-10-25

### Added
- **Smart Hint Card Wrapping, Centering, and Multi-Position Screen HUD (`BadgeHudPosition`)**:
  - Implemented multi-row and multi-column wrapping for offscreen and covered window hint cards (HUD pills) in modal Jump Mode (`Meta+Space`) via `OutlineBadge::computeHudPillPositions`.
  - When many windows or targets are present, cards automatically wrap across multiple rows or columns and each line is centered horizontally (for Top/Bottom) or vertically (for Left/Right), eliminating overflow and clipping off screen edges.
  - Added the **Hint cards HUD position** configuration option (`kcfg_BadgeHudPosition`) in the Marked Windows settings tab with 4 positions:
    - `0`: Bottom (horizontal, centered rows) — default.
    - `1`: Top (horizontal, centered rows).
    - `2`: Left (vertical, centered columns).
    - `3`: Right (vertical, centered columns).
- **Custom Window Labels for Marked Windows (`Meta+Ctrl+Shift+Space`)**:
  - Added an optional "Custom label (optional) / Własna etykieta" text field to the Custom Mark dialog (`active-window-outline-colorpicker`).
  - Allows assigning custom names/tags (e.g. "Backend", "Terminal", "Projekt A") to marked windows.
  - Displayed prominently in **Hint Cards** ("karty podpowiedzi") during Jump Mode (`Meta+Space`): Line 1 shows the bold white label, and Line 2 displays the location and window title (e.g. `Pulpit 1 · bash` or `Schowane · Alacritty`), solving the ambiguity when multiple windows share identical process/window names.
  - Automatically reflected on window letter badges (`[ A | Label ]`) when labels are defined, with smart width calculation and contrasting text separator.
  - Existing labels are prefilled when re-opening the custom mark dialog, allowing easy editing or clearing.
- **Redesigned Companion System Tray Icon & Standalone Settings GUI (`active-window-outline-tray` & `active-window-outline-settings`)**:
  - Dedicated lightweight Qt6 companion tray application with custom-drawn dynamic glowing icon (reflecting active outline state and accent color).
  - Single left click on the tray icon toggles the outline effect on and off immediately.
  - Redesigned, clean and functional tray menu:
    - Primary outline toggle on/off (`Meta+Ctrl+O`) with real-time status indicator.
    - **Presets Submenu**: Select and activate presets (built-in & custom) with radio-style checkmark, 1-click **Save current as new preset (with timestamp: `Preset_YYYY-MM-DD_HH-mm-ss`)**, **Export preset to file**, and **Import preset**.
    - **Keyboard Shortcuts Submenu**: Displays all shortcuts with key bindings and triggers them on click: Jump to Window (`Meta+Space`), Mark / Unmark (`Meta+Ctrl+M`), Custom Mark Palette (`Meta+Ctrl+Shift+M`), Toggle Outline (`Meta+Ctrl+O`), Previous Window (`Meta+Alt+[`), Next Window (`Meta+Alt+]`), Clear All Marks, and an interactive **Shortcuts Cheat Sheet** dialog.
    - **Check for Updates**: One-click asynchronous update check against GitHub releases API with visual feedback and direct release page link.
    - **Settings Launcher**: Opens the full configuration dialog in-process (`SettingsDialog`) and provides a standalone executable `active-window-outline-settings` (`active-window-outline-settings.desktop`), completely resolving the KCM plugin loader error (`could not find plugin kwin_active-window-outline`).
  - Added `TrayIconEnabled` checkbox in the Appearance (General) settings tab (enabled by default) and autostart desktop entry (`~/.config/autostart/active-window-outline-tray.desktop`).
  - Bi-directional synchronization: exports `org.kde.ActiveWindowOutline` D-Bus interface with `stateChanged` signal and automatic fallback to `kwinrc` file watching and `reconfigureEffect`.
- **Full Configuration Preset Management System & Presets Tab**:
  - Added a dedicated **Presets** tab (`tabPresets`) in the configuration module for managing complete outline presets.
  - Ships with 9 curated built-in presets: `LeapK Default (v1.5)`, `Vim HUD Navigator`, `Subtle Minimalist`, `Nordic Frost & Glow`, `Cyberpunk Neon 2077`, `Ambilight Dynamic Flow`, `Tiling Matrix Focus Trail`, `RGB Chroma Spectrum`, and `High-Contrast Accessibility`.
  - Presets list displays name, description, type (Built-in vs User), and includes an inline **Export** button on each row.
  - Quick action toolbar: **Activate Preset**, **New Preset…**, **Duplicate…**, **Delete**, **Import Preset…**, and **Export Preset…**.
  - Double-clicking any preset in the table instantly applies and activates it.
  - Persistent global **Save Preset…** button (`btnSavePresetGlobal`) placed at the bottom of the configuration dialog, allowing saving current settings as a preset from any tab.
  - Native JSON serialization (`presethelper.h`) for portable importing and exporting of preset files.
- **Left-Aligned Hint Cards for Side HUD Positions (`BadgeHudPosition` 2 & 3)**:
  - Cards positioned at the Left or Right edges of the screen now align strictly to the left edge of each column instead of centering, ensuring jump letter badges (`[A]`, `[B]`, `[C]`) form a straight vertical line.
- **Dynamic RAM Throttling & OOM Protection in Installers**:
  - `install-leapk.sh` dynamically checks total physical memory and caps parallel compiler jobs (`min(cpus, mem_gb / 2, 4)`), preventing compiler crashes and desktop service kills on low-memory or high-core machines.
- **Screen Edge Window Filtering in Jump Mode**:
  - Excluded 1-pixel screen edge trigger and helper windows (`< 30x30` px, popups, and skip-switcher windows) from receiving leap targets, preventing letter badges from showing cut in half on screen boundaries.
- **Complete Legacy Launcher & Desktop Cache Purge**:
  - Automatically uninstalls and cleans up stale `.desktop` launchers and autostart files from previous versions and reindexes KDE services via `kbuildsycoca6 --noincremental`.
- **Dedicated "About" Tab in System Settings KCM with Update & Reset Controls**:
  - Moved the "Check for updates" button and status display from the General Appearance tab into a dedicated, clean **About** tab.
  - Added a "Reset to Defaults" button (`btnResetDefaults`) with confirmation dialog to easily restore all effect options, colors, shaders, and shortcuts to their default values.
  - Included detailed plugin metadata, author attribution, and clickable GitHub repository link.
- **Configurable Mark Options in Shortcut Dialog (`Meta+Ctrl+Shift+M`)**:
  - Added checkboxes for both "Mark with fixed letter" and "Custom mark color" in the mark settings dialog.
  - Allows marking windows with a custom outline color without binding a fixed letter (candidates dynamically receive available letters from the keyboard pool during Jump Mode), or assigning a fixed letter while using default/random mark colors.
  - Automatically disables/enables input controls and dynamically updates the color button preview.
  - Both options default to checked, and user selections are persisted across sessions via settings.
- **Multi-Desktop Window Jumping & Offscreen HUD Cards (`Meta+Space`)**:
  - Modal Jump Mode now functions across all virtual desktops. Marked windows on other desktops and minimized windows retain their fixed letters and can be jumped to instantly from any virtual desktop.
  - Jumping to a window residing on a different desktop automatically switches virtual desktops (`effects->setCurrentDesktop`) and activates/unminimizes the window in one smooth step.
  - Floating Offscreen Jump Targets HUD: When Jump Mode is active and marked windows (or offscreen targets) exist on other desktops or in minimized state, an elegant, non-intrusive HUD bar appears at the bottom-center of the screen displaying each target's letter badge, desktop name/number (e.g. `Desktop 2`), and window title.
  - Expanded `JumpTargets` scope options in KCM settings:
    - `0`: Current desktop visible windows + marked windows from all desktops (default).
    - `1`: Marked windows only (all desktops).
    - `2`: All windows across all virtual desktops.
- **Smart Adaptive (Hybrid) Accent Detection for Marked Windows (`MarkSmartAdaptive`)**:
  - Added a "Smart adaptive hybrid accent" checkbox in KCM settings under Marked Windows, enabled (`true`) by default over random color.
  - Automatically extracts the mark outline color using the intelligent Smart Adaptive (Hybrid) ambilight algorithm from live window content and application icon (`OutlineBadge::extractSmartAccentFromImage`).
  - When marking a window via shortcut (`Meta+Ctrl+M`), the mark outline color is dynamically set to the detected window accent.
  - In the custom mark dialog (`Meta+Ctrl+Shift+M`), the color picker button and initial color are pre-selected to the detected Smart Adaptive accent automatically.
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
  - In modal Jump Mode (`Meta+Space`), dynamically selects alternate visible corners/center if the primary position is covered; completely occluded windows do not display confusing floating badges.
  - Real-time full repaint when stacking order or virtual desktop changes (`stackingOrderChanged`, `desktopChanged`).

### Changed
- **Rebranded Project to LeapK (`LeapK – Vim-style Window Navigator & Window Highlighter for KDE`)**:
  - Rebranded the project from *Active Window Outline* / *KAWO* to **LeapK** to reflect its evolution into a full keyboard-driven window navigation, modal leaping, and focus highlight engine.
  - Shortcut prefix renamed from `KAWO:` to `LeapK:` in KDE System Settings (e.g. `LeapK: Jump to Window`, `LeapK: Toggle Window Highlight`, `LeapK: Toggle Mark Window`).
  - Standalone tools and executables now available as `leapk` / `leapk-settings` and `leapk-tray` (with backward compatibility symlinks `kleap-*` and `active-window-outline-*`).
  - Added installer/uninstaller aliases `install-leapk.sh` and `uninstall-leapk.sh`.
  - Added vector icon `leapk.svg` installed to the system hicolor scalable icon theme and embedded in Qt resources.
  - Companion tray and settings window updated to LeapK branding with multi-interface D-Bus support (`org.kde.LeapK`, `org.kde.KLeap` & `org.kde.ActiveWindowOutline`).
  - Default built-in preset renamed to `LeapK Default (v1.5)`.
- **Default Focus History Navigation Shortcuts Updated to `Meta+Ctrl+[` and `Meta+Ctrl+]`**:
  - Changed default previous recent window shortcut from `Meta+Alt+[` to `Meta+Ctrl+[`.
  - Changed default next recent window shortcut from `Meta+Alt+]` to `Meta+Ctrl+]`.
  - Avoids common window manager and application Alt key conflicts while providing ergonomic focus navigation.
- **Ergonomic Space-based Default Shortcuts for Window Marking**:
  - Changed default toggle mark window shortcut from `Meta+Ctrl+M` to `Meta+Ctrl+Space`.
  - Changed default custom mark window shortcut from `Meta+Ctrl+Shift+M` to `Meta+Ctrl+Shift+Space`.
  - Conveniently unifies all window navigation and marking operations around the `Space` key: Jump to Window (`Meta+Space`), Toggle Mark Window (`Meta+Ctrl+Space`), and Custom Mark Window (`Meta+Ctrl+Shift+Space`).
- **Scrollable Settings Dialog & Dynamic Screen Sizing**:
  - Embedded each configuration tab page inside an independent `QScrollArea` with automatic vertical scrollbars as needed, eliminating dialog height overflow.
  - Dynamically sized `SettingsDialog` based on primary screen available geometry (`avail.height() - 80`), ensuring dialogs fit comfortably on 1080p, 768p, and high-DPI scaled screens while keeping bottom action buttons (`Ok`, `Apply`, `Cancel`, `Save Preset…`) visible and accessible at all times.
- **Dynamic Shortcut Synchronization Across Tray & Cheatsheet**:
  - Live query of active system shortcuts using `KGlobalAccel::globalShortcut` with `kglobalshortcutsrc` disk reparse fallback for all KAWO actions (`JumpToWindow`, `MarkActiveWindow`, `MarkActiveWindowWithColor`, `ToggleActiveWindowOutline`, `PrevRecentWindow`, `NextRecentWindow`).
  - Dynamically updates tray menu item texts and the interactive Shortcuts Cheat Sheet dialog to reflect whatever key combinations are currently bound in KDE System Settings.
  - Added filesystem watcher on `kglobalshortcutsrc` for instantaneous updates when shortcuts are changed by the user.
- **Robust Real-Time Outline State & Tray Icon Synchronization**:
  - Query live KWin effect outline state directly via D-Bus `isOutlineEnabled()` with `reparseConfiguration()` fallback, fixing an issue where cached `KSharedConfig` state caused the tray icon and menu checkbox to stay static.
  - Accurately toggles the "Outline is Enabled" checkmark in the tray menu and immediately transitions the tray icon between the glowing neon active state and the dimmed dashed disabled state.
- **Tray Icon & Launcher Search Refinements**:
  - Removed "Quit Tray" / "Zamknij tackę" from the tray context menu; enabling and disabling the tray is now managed exclusively from Settings.
  - Enhanced tray icon rendering with multi-layer glowing neon halo and vibrant accent outline when enabled, switching to a dimmed dashed outline when disabled.
  - Reordered window titlebar control dots to the right side (`Minimize`, `Maximize`, `Close`), matching KDE Plasma's native window decoration layout.
  - Hidden tray daemon from launcher search results (`NoDisplay=true` in `active-window-outline-tray.desktop`), keeping only `Active Window Outline Settings` visible in KRunner and application menus.
  - Added custom vector icon `active-window-outline.svg` installed to hicolor scalable icon theme and embedded via Qt resources for standalone settings and tray.
  - Added `setStaleLockTime(0)` to `QLockFile` for instant single-instance daemon recovery upon restarts.
- **Updated Default Outline & Marked Settings (v1.5)**:
  - `MarkSmartAdaptive` is now enabled (`true`) by default to automatically extract window accents via Smart Adaptive (Hybrid) mode (`MarkRandomColor` defaults to `false`).
  - `BadgePosition` default moved to Top (`1`, centered on top edge) and `BadgeMargin` reduced to `0 px`.
  - Inactive marked windows now default to `MarkInactiveDashed = true`, `MarkInactiveDashSmooth = true`, and a `1 px` border reduction (`MarkInactiveWidthReduction = 1`).
  - Appearance default: `GlowEnabled` (Soft glow) disabled (`false`) by default for crisp modern lines.
  - Delay buffers: `MenuHideDelay` and `MenuShowDelay` defaults increased to `500 ms` (from 300 ms).
- **Default Jump Mode Shortcut Changed to `Meta+Space`**:
  - Replaced `Meta+Alt+Space` with the simpler and standard `Meta+Space` shortcut for triggering modal Jump to Window mode (`JumpToWindow`).
  - The installer automatically migrates existing configurations to `Meta+Space`.
- **Default Style Precedence Prioritizes Marked Windows (`StylePrecedence`)**:
  - Changed default outline style precedence order from `Active,Marked,Recent` to `Marked,Active,Recent`.
  - Marked windows now maintain their custom mark color and styling by default even when focused, rather than being overridden by the standard active window outline.

### Fixed
- **Multi-Monitor Desktop Surface Outlining & Marking Filtering (`isEligibleOutlineWindow`)**:
  - Fixed an issue where clicking on or transitioning between multi-monitor desktop backgrounds (e.g. secondary monitor wallpaper) caused desktop background windows (`isDesktop()`, `isDock()`, `isSpecialWindow()`) to be marked, delay-cached, or outlined around the entire screen.
  - Added unified `isEligibleOutlineWindow` validation across active tracking, window marking, delay-fade state, recent focus history, and paint rendering.
- **Build Failure on KWin < 6.6 (`Rect` Undeclared in `sampleAmbilightColor`)**:
  - Fixed a compilation error on older KWin releases (< 6.6) where `Rect` was not declared. Introduced `OutlineRect` compatibility alias (`QRect` on legacy KWin, `KWin::Rect` on modern KWin).
- **Spurious Intermediate Window Activation on Cross-Desktop Jump (`m_expectedJumpTarget`)**:
  - Fixed an issue where jumping across virtual desktops via modal Jump Mode (`Meta+Space`) caused KWin to briefly activate the last active window on the destination desktop, inserting it into focus history and intercepting `Meta+Alt+[` navigation.
  - Added `m_expectedJumpTarget` guard in `slotWindowActivated` ensuring intermediate workspace desktop activations are ignored until the selected target window activates.
  - Navigating back via `Meta+Alt+[` now returns directly to the window that was active before the jump across desktops as expected.
- **Covered Marked Windows Displayed as Bottom HUD Cards (`renderOffscreenJumpTargets`)**:
  - Fixed an issue where marked windows completely hidden or covered under other windows on the current desktop had their letter badges dropped and became inaccessible during modal Jump Mode (`Meta+Space`).
  - Now, if all 8 alternative badge positions are occluded by overlapping windows, covered windows are routed to the bottom HUD pill bar alongside offscreen desktop and minimized targets, displaying their letter badge, title, and a localized `Schowane` / `Covered` status card.
  - Pressing the letter key immediately raises and activates the covered window to the foreground.
  - Also expanded alternate badge placement check across all 8 cardinal positions in continuous badges mode.
- **High-Contrast Badge Letter Text & Framing Border (`contrastingTextColor`)**:
  - Replaced naive HSL lightness check with ITU-R BT.601 perceptual luminance formula (`0.299 R + 0.587 G + 0.114 B`).
  - Dark badge backgrounds (e.g. dark jump badge, red, deep violet, sky blue) strictly render crisp white letters, while light backgrounds (e.g. sunflower yellow, warm yellow, cyan, bright teal, emerald green, light gray) render crisp black letters.
  - Dynamically adjusts the badge framing border between dark (for light backgrounds) and white (for dark backgrounds) to prevent light badges from washing out on bright window content.
- **Plugin Toggle Shortcut (`Meta+Ctrl+O`) Now Disables All Visual Elements & Badges**:
  - Fixed an issue where toggling off the effect via `ToggleActiveWindowOutline` (`Meta+Ctrl+O`) only removed the active window outline while continuous badges/hints, recent outlines, and marked window outlines stayed painted on screen.
  - Guarded `isActive()`, `prepareScreenPaint()`, `paintScreen()` (recent, marked, and continuous badge passes), and `slotTriggerJumpMode()` with strict `m_outlineEnabled` checks.
  - Toggling off now terminates active Jump Mode, stops all animation/hide timers, clears cached outline data, and triggers a full screen repaint (`effects->addRepaintFull()`), cleanly clearing all overlays from the display.
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
