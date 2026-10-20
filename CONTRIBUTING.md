# Contributing

Thank you for helping improve KWin Active Window Outline.

## Reporting bugs

Please include:

- distribution and version;
- KDE Plasma, KWin, Qt, and KDE Frameworks versions;
- Wayland or X11 session;
- GPU and driver;
- steps to reproduce the problem;
- expected and actual behavior;
- relevant output from the user journal if available.

For rendering issues, include a screenshot or short screen recording and state whether **Draw inside window** is enabled.

For menu or panel problems on version 1.2.10 or later, run
`bash collect-outline-diagnostics.sh` from the source directory and reproduce
the problem during its 20-second capture. Review and attach the resulting log.
It contains geometry, window flags and a build fingerprint, but no window
titles or content. Run it in your normal Plasma session, without `sudo`.

## Development

Build the Wayland version without installing it:

```bash
cmake -S . -B build-wayland \
  -DKWIN_BACKEND=WAYLAND \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-wayland --parallel
```

To build and run the regression tests (no running compositor needed):

```bash
cmake -S . -B build-wayland -DOUTLINE_BUILD_TESTS=ON
cmake --build build-wayland --parallel
ctest --test-dir build-wayland --output-on-failure
```

These tests cover the ungrabbed-popup fallback, color sequence parsing and CSS easing interpolation, procedural shader mathematical models (breathe, neon, flash, hologram), rounded-rectangle VBO strip tessellation, and automated synchronization of KConfig settings with installer and uninstaller scripts. On Wayland with KWin 6.6+, cursor tests also exercise the real scene filters, including output isolation, cleanup on an interrupted pass, and preservation of hardware-cursor exclusions. They do not replace runtime rendering tests in Plasma.

Run `shellcheck` on the shell scripts and validate `metadata.json`, the `.ui` file, and the KConfig schema before opening a pull request.

## Code style and scope

- Keep changes focused and avoid unrelated formatting rewrites.
- Preserve SPDX headers and GPL-3.0-or-later compatibility.
- Do not claim compatibility with a KWin version or backend that was not tested.
- Document user-visible changes in `CHANGELOG.md`.

By submitting a contribution, you agree to license it under GPL-3.0-or-later.
