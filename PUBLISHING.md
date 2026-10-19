# Publishing checklist

## Recommended GitHub repository

- Repository name: `kwin-active-window-outline`
- Description: `A theme-independent active window outline effect for KDE Plasma 6 and KWin.`
- Website: leave empty until a KDE Store page exists
- License: GPL-3.0
- Suggested topics: `kde`, `kde-plasma`, `kwin`, `kwin-effect`, `plasma-6`, `wayland`, `window-manager`, `accessibility`, `focus`

## Before each release

- Keep screenshots representative of the current appearance and defaults.
- Set `VERSION` and `KPlugin.Version` in `metadata.json` to the same version.
- Move the relevant changelog entries into a dated release section.
- Build with `OUTLINE_BUILD_TESTS=ON` and run CTest as documented in
  [CONTRIBUTING.md](CONTRIBUTING.md). Run ShellCheck on the shell scripts.
- Confirm the effect on a clean Plasma 6 Wayland session.
- Test installation, update, logout/login, configuration, and uninstallation.
- Check floating and non-floating panels, application context menus,
  Firefox context menus, KWin's titlebar menu, and hover tooltips.
- Record only the KWin versions and systems actually tested. A passing build
  is not proof of correct rendering or binary compatibility on another system.
- Commit and push the release sources, then create a matching version tag
  (for example, `v1.2.11`) at that commit. Do not move published release tags.
- Create the GitHub release from that tag. GitHub provides source ZIP and
  tar.gz downloads automatically; do not upload local test archives or binaries
  as if they were universal builds.
- Copy the matching changelog section into the release notes.
- Remind users to log out and back in after updating a native KWin plugin.

## Distribution status

Version 1.2.11 is a source release with an installation script. Native package
recipes, automatic package builds, and OBS repositories are future work; do not
advertise them as available until they have been implemented and tested.

## Optional KDE Store listing

Use the Plasma 6 **KWin Effects** category. Because this project is a native C++ plugin, explain that it must be compiled against the installed KWin version and cannot be installed as a normal one-click QML effect. Link to the GitHub releases and installation instructions.
