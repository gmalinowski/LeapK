# Publishing checklist

## Recommended GitHub repository

- Repository name: `kwin-active-window-outline`
- Description: `A theme-independent active window outline effect for KDE Plasma 6 and KWin.`
- Website: leave empty until a KDE Store page exists
- License: GPL-3.0
- Suggested topics: `kde`, `kde-plasma`, `kwin`, `kwin-effect`, `plasma-6`, `wayland`, `window-manager`, `accessibility`, `focus`

## Before the first public release

- Add a screenshot or short GIF showing a focused and unfocused window.
- Confirm the effect on a clean Plasma 6 Wayland session.
- Test installation, update, logout/login, configuration, and uninstallation.
- Create a signed or annotated `v1.2.6` tag.
- Attach the source archive generated from that tag to the GitHub release.
- Copy the matching changelog section into the release notes.

## Optional KDE Store listing

Use the Plasma 6 **KWin Effects** category. Because this project is a native C++ plugin, explain that it must be compiled against the installed KWin version and cannot be installed as a normal one-click QML effect. Link to the GitHub releases and installation instructions.
