# LeapK – Vim-style Window Navigator & Smart Focus Border for KDE

[![KDE Plasma 6](https://img.shields.io/badge/KDE%20Plasma-6.x-blue.svg?logo=kde)](https://kde.org)
[![Wayland Ready](https://img.shields.io/badge/Wayland-Native-success.svg)](https://wayland.freedesktop.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/gmalinowski/LeapK?logo=github)](https://github.com/gmalinowski/LeapK/releases/latest)

> **LeapK** is a native KWin compositor effect and companion tool for **KDE Plasma 6** that combines keyboard-driven Vim-style window navigation with a hardware-accelerated, customizable active window border and focus history indicator.
>
> 💡 *Pairs seamlessly with dynamic tiling window managers like [**Krohnkite**](https://github.com/esjeon/krohnkite) (which is featured in the preview screencasts below).*

---

## ⚡ TL;DR

### What is it?
- **Teleport between windows with zero mouse movement**: Press `Meta+Space`, spot the letter badge on your target window (even on another virtual desktop or minimized), and tap that key to jump instantly.
- **Never lose keyboard focus again**: Draws a crisp, theme-independent outline around the active window, plus an optional fading focus trail (1–5 previous windows) for seamless multitasking and tiling layouts.
- **Tag & personalize windows**: Pin custom letters and readable labels like `[ A | Backend ]` or `[ T | Terminal ]` with personalized colors.
- **Built-in aesthetic presets**: 9 curated visual profiles from ultra-clean 1px hairlines to reactive Ambilight dynamic glows and Cyberpunk neon.

### Quick Install (1-Liner)
Run in your terminal to automatically install dependencies, compile, and enable LeapK:
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash
```
*(After the first install or update, log out and back into Plasma so KWin loads the new native plugin).*

---

## 🎥 Preview
*(Captured with the **Krohnkite** tiling script enabled).*

[jumping.webm](https://github.com/user-attachments/assets/fd9e127e-caf3-4dff-b378-5053942b7874)


[leapk-outline-181750_comp.webm](https://github.com/user-attachments/assets/0f3e7ddb-b5ff-4209-98eb-15f93def59a3)

[settings.webm](https://github.com/user-attachments/assets/f0bbab7b-bdcf-4cac-920d-4083aad8ccbb)

---

## ⌨️ Default Keyboard Shortcuts

| Shortcut | Action | Description |
| :--- | :--- | :--- |
| **`Meta + Space`** | **Jump to Window** | Enters modal Leap mode: shows letter badges on windows; press a letter to jump immediately. |
| **`Meta + Space Space`** | **Jump to Recent Window** | Instantly switches to the most recently active window (`recent`). Works by tapping `Space` in Jump mode or double-pressing `Meta+Space`. |
| **`Meta + Ctrl + Space`** | **Quick Mark Window** | Toggles a persistent mark outline and letter badge on the current window. |
| **`Meta + Ctrl + Shift + Space`** | **Mark with Label & Color** | Opens the native mark dialog to set a custom label (e.g. `Server`), letter, and color. |
| **`Meta + Ctrl + O`** | **Toggle LeapK** | Instantly toggles the outline effect and overlays on or off. |
| **`Meta + Ctrl + [`** | **Previous Window** | Cycles backward through recent focus history across all virtual desktops. |
| **`Meta + Ctrl + ]`** | **Next Window** | Cycles forward through recent focus history. |

*All shortcuts can be customized in **System Settings → Shortcuts → KWin** (search for `LeapK`).*

---

## ✨ Features at a Glance

### 🚀 Vim-Style Window Jumping & Multi-Position HUD
- **Instant Leaping & Recent Switch**: Press `Meta+Space` to display high-contrast letter badges on background windows. Hitting the letter raises, unminimizes, and focuses the target window instantly. Tapping `Space` (giving `Meta+Space Space`) or double-pressing `Meta+Space` immediately switches to your last active window (`recent`).
- **Cross-Desktop & Minimized Navigation**: Windows on other virtual desktops or hidden behind other windows appear in an elegant **HUD Pill Bar** at your chosen screen edge (**Bottom**, **Top**, **Left**, or **Right**) with auto-wrapping and centering.
- **Window Labels**: Add semantic tags (e.g. `[ A | Work ]`, `[ B | Logs ]`) so you never confuse identical terminal or browser windows.

### 🖼️ Active & Recent Window Focus Trail
- **Active Window Outline**: Follows window focus at the compositor level with zero input latency and zero CPU/GPU overhead when idle.
- **Focus Depth Trail**: Highlights 1 to 5 previously active windows with smooth **opacity decay**, **grayscale desaturation**, and **dashed borders with expanding gaps**, giving an unmistakable sense of temporal depth.
- **Smart Popup Suppression**: Automatically hides the border during context menus, dropdowns, and Plasma panel popups.

### 🎨 Ambilight, Shaders & Contrast Engine
- **Smart Adaptive Ambilight**: Samples dominant colors from live window pixels and application icons in real time.
- **Perimeter Contrast Enforcement**: Monitors window borders and wallpaper colors, dynamically boosting contrast so outlines never wash out.
- **Procedural Shaders**: Optional *Soft Glow*, *Breathe / Pulse*, *Futuristic Hologram*, and *Neon Flicker*.
- **Modular Custom GLSL Shader Effects**: Create, edit, import, and export custom shader effects (`.leapk-shader` / `.json`) with dynamic uniform parameters (sliders, spinboxes, colors). JIT-compiled in real time by KWin without ahead-of-time toolchains. Bundles starter shaders: *Rainbow Wave*, *Matrix Digital Rain*, *Electric Plasma*, and *Fire Aura*.

### 🎛️ 9 Curated Presets & Presets Manager
Switch styles instantly from the tray menu or configuration dialog:
- **LeapK Default (v1.5)**: Clean modern accent outline with top badges and subtle glow.
- **Vim HUD Navigator**: Top HUD badges and focus history trail designed for keyboard ninjas.
- **Subtle Minimalist**: 1px hairline border, 4px corners, zero distractions.
- **Nordic Frost & Glow**: Ice-blue `#88c0d0` palette with gentle breathing pulsation.
- **Cyberpunk Neon 2077**: Electric cyan `#00f0ff` with 14px glow and glitch scanlines.
- **Ambilight Dynamic Flow**: Real-time 3.0 Hz dynamic color matching.
- **Tiling Matrix Focus Trail**: Emerald green with 3-window dashed focus history for tiling WMs.
- **RGB Chroma Spectrum**: Chromatic cycling animation across the color wheel.
- **High-Contrast Accessibility**: Bold 4px amber `#f39c12` border with extra-large 34px badges.
- *Export and import presets via portable JSON files to share with the community.*

### 🧰 Companion Tray App & Standalone Settings
- Lightweight Qt6 system tray icon reflecting active state and current color.
- Dedicated settings application (`leapk-settings`) for instant configuration without KCM plugin issues.

---

## 📦 Installation

### Option 1: Quick Install / Update (Recommended)

**Latest Stable Release (Default):**
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash
```

**Latest Beta / Pre-release:**
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash -s -- --pre
```

**Specific Version (e.g. `v1.5.0`, `v1.4.25`):**
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash -s -- --tag v1.5.0
```

**Latest Development (Bleeding edge `main` branch):**
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash -s -- --main
```

### Option 2: Build from Source
```bash
git clone https://github.com/gmalinowski/LeapK.git
cd LeapK
bash install-leapk.sh
```
*The installer automatically detects your distribution package manager (`pacman`, `apt`, `dnf`, `zypper`), installs required Qt6/KF6 dependencies, limits compiler RAM usage to protect against OOM, compiles the native KWin plugin, and enables it.*

> [!NOTE]
> **Requirements**: KDE Plasma 6.x / KWin 6.x on Wayland (recommended) or X11.

---

## ⚙️ Configuration

You can access settings in three ways:
1. Click the **LeapK** icon in your system tray → **Settings…**
2. Run `leapk-settings` in your terminal or application launcher (Kickoff / KRunner).
3. Open **System Settings → Window Management → Desktop Effects** and click the gear icon next to **LeapK**.

---

## 🔄 Updating & Uninstallation

### To Update:
Simply re-run the quick install command:
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash
```
*(Your custom settings and presets are automatically preserved).*

### To Uninstall:
Run the uninstaller script to cleanly remove all binaries, desktop files, tray autostarts, and KWin shortcuts:
```bash
curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/uninstall.sh | bash
```
*(Or locally from repository: `bash uninstall-leapk.sh`).*

---

## ❓ FAQ & Troubleshooting

<details>
<summary><b>The outline does not appear after installation or update</b></summary>

Native KWin plugins are cached in memory. After updating or installing for the first time, log out of your desktop session and log back in. Also ensure the effect is checked in **System Settings → Desktop Effects**.
</details>

<details>
<summary><b>Outlines disappear on maximized windows</b></summary>

By default, outlines are drawn inside the window border. If you configured outside borders, they fall outside screen bounds when maximized. In settings, make sure **Draw inside window** is enabled.
</details>

<details>
<summary><b>A shortcut conflicts with an existing application</b></summary>

Open **System Settings → Shortcuts → KWin**, search for `LeapK`, and assign any custom keybinding.
</details>

---

## 📜 Origin & License

The rendering foundation was originally adapted from [Powermayer/keep-above-outline](https://github.com/Powermayer/keep-above-outline) by Matthias Bauer. LeapK expands this into a full keyboard navigation system with modal leap navigation, focus history trails, dynamic color sampling, and multi-desktop HUD management.

Licensed under the **[GNU General Public License v3.0 or later](LICENSE)**.
